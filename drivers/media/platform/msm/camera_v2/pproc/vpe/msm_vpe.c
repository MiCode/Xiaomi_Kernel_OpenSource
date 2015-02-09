/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "MSM-VPE %s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/msm_ion.h>
#include <linux/iommu.h>
#include <mach/iommu_domains.h>
#include <mach/iommu.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/media-entity.h>
#include <media/msmb_generic_buf_mgr.h>
#include <media/msmb_pproc.h>
#include "msm_vpe.h"
#include "msm_camera_io_util.h"

#define MSM_VPE_IDENT_TO_SESSION_ID(identity) ((identity >> 16) & 0xFFFF)
#define MSM_VPE_IDENT_TO_STREAM_ID(identity) (identity & 0xFFFF)

#define MSM_VPE_DRV_NAME "msm_vpe"

#define MSM_VPE_MAX_BUFF_QUEUE 16

#define CONFIG_MSM_VPE_DBG 0

#if CONFIG_MSM_VPE_DBG
#define VPE_DBG(fmt, args...) pr_err(fmt, ##args)
#else
#define VPE_DBG(fmt, args...) pr_debug(fmt, ##args)
#endif

static void vpe_mem_dump(const char * const name, const void * const addr,
			int size)
{
	char line_str[128], *p_str;
	int i;
	u32 *p = (u32 *) addr;
	u32 data;
	VPE_DBG("%s: (%s) %p %d\n", __func__, name, addr, size);
	line_str[0] = '\0';
	p_str = line_str;
	for (i = 0; i < size/4; i++) {
		if (i % 4 == 0) {
			snprintf(p_str, 12, "%08x: ", (u32) p);
			p_str += 10;
		}
		data = *p++;
		snprintf(p_str, 12, "%08x ", data);
		p_str += 9;
		if ((i + 1) % 4 == 0) {
			VPE_DBG("%s\n", line_str);
			line_str[0] = '\0';
			p_str = line_str;
		}
	}
	if (line_str[0] != '\0')
		VPE_DBG("%s\n", line_str);
}

static inline long long vpe_do_div(long long num, long long den)
{
	do_div(num, den);
	return num;
}

#define msm_dequeue(queue, member) ({					\
			unsigned long flags;				\
			struct msm_device_queue *__q = (queue);		\
			struct msm_queue_cmd *qcmd = 0;			\
			spin_lock_irqsave(&__q->lock, flags);		\
			if (!list_empty(&__q->list)) {			\
				__q->len--;				\
				qcmd = list_first_entry(&__q->list,	\
							struct msm_queue_cmd, \
							member);	\
				list_del_init(&qcmd->member);		\
			}						\
			spin_unlock_irqrestore(&__q->lock, flags);	\
			qcmd;						\
		})

static void msm_queue_init(struct msm_device_queue *queue, const char *name)
{
	spin_lock_init(&queue->lock);
	queue->len = 0;
	queue->max = 0;
	queue->name = name;
	INIT_LIST_HEAD(&queue->list);
	init_waitqueue_head(&queue->wait);
}

static struct msm_cam_clk_info vpe_clk_info[] = {
	{"vpe_clk", 160000000},
	{"vpe_pclk", -1},
};

static int msm_vpe_notify_frame_done(struct vpe_device *vpe_dev);

static void msm_enqueue(struct msm_device_queue *queue,
			struct list_head *entry)
{
	unsigned long flags;
	spin_lock_irqsave(&queue->lock, flags);
	queue->len++;
	if (queue->len > queue->max) {
		queue->max = queue->len;
		pr_debug("queue %s new max is %d\n", queue->name, queue->max);
	}
	list_add_tail(entry, &queue->list);
	wake_up(&queue->wait);
	VPE_DBG("woke up %s\n", queue->name);
	spin_unlock_irqrestore(&queue->lock, flags);
}

static struct msm_vpe_buff_queue_info_t *msm_vpe_get_buff_queue_entry(
	struct vpe_device *vpe_dev, uint32_t session_id, uint32_t stream_id)
{
	uint32_t i = 0;
	struct msm_vpe_buff_queue_info_t *buff_queue_info = NULL;

	for (i = 0; i < vpe_dev->num_buffq; i++) {
		if ((vpe_dev->buff_queue[i].used == 1) &&
			(vpe_dev->buff_queue[i].session_id == session_id) &&
			(vpe_dev->buff_queue[i].stream_id == stream_id)) {
			buff_queue_info = &vpe_dev->buff_queue[i];
			break;
		}
	}

	if (buff_queue_info == NULL) {
		pr_err("error buffer queue entry for sess:%d strm:%d not found\n",
			session_id, stream_id);
	}
	return buff_queue_info;
}

static unsigned long msm_vpe_get_phy_addr(struct vpe_device *vpe_dev,
	struct msm_vpe_buff_queue_info_t *buff_queue_info, uint32_t buff_index,
	uint8_t native_buff)
{
	unsigned long phy_add = 0;
	struct list_head *buff_head;
	struct msm_vpe_buffer_map_list_t *buff, *save;

	if (native_buff)
		buff_head = &buff_queue_info->native_buff_head;
	else
		buff_head = &buff_queue_info->vb2_buff_head;

	list_for_each_entry_safe(buff, save, buff_head, entry) {
		if (buff->map_info.buff_info.index == buff_index) {
			phy_add = buff->map_info.phy_addr;
			break;
		}
	}

	return phy_add;
}

static unsigned long msm_vpe_queue_buffer_info(struct vpe_device *vpe_dev,
	struct msm_vpe_buff_queue_info_t *buff_queue,
	struct msm_vpe_buffer_info_t *buffer_info)
{
	struct list_head *buff_head;
	struct msm_vpe_buffer_map_list_t *buff, *save;
	int rc = 0;

	if (buffer_info->native_buff)
		buff_head = &buff_queue->native_buff_head;
	else
		buff_head = &buff_queue->vb2_buff_head;

	list_for_each_entry_safe(buff, save, buff_head, entry) {
		if (buff->map_info.buff_info.index == buffer_info->index) {
			pr_err("error buffer index already queued\n");
			return -EINVAL;
		}
	}

	buff = kzalloc(
		sizeof(struct msm_vpe_buffer_map_list_t), GFP_KERNEL);
	if (!buff) {
		pr_err("error allocating memory\n");
		return -EINVAL;
	}

	buff->map_info.buff_info = *buffer_info;
	buff->map_info.ion_handle = ion_import_dma_buf(vpe_dev->client,
		buffer_info->fd);
	if (IS_ERR_OR_NULL(buff->map_info.ion_handle)) {
		pr_err("ION import failed\n");
		goto queue_buff_error1;
	}

	rc = ion_map_iommu(vpe_dev->client, buff->map_info.ion_handle,
		vpe_dev->domain_num, 0, SZ_4K, 0,
		(unsigned long *)&buff->map_info.phy_addr,
		&buff->map_info.len, 0, 0);
	if (rc < 0) {
		pr_err("ION mmap failed\n");
		goto queue_buff_error2;
	}

	INIT_LIST_HEAD(&buff->entry);
	list_add_tail(&buff->entry, buff_head);

	return buff->map_info.phy_addr;

queue_buff_error2:
	ion_unmap_iommu(vpe_dev->client, buff->map_info.ion_handle,
		vpe_dev->domain_num, 0);
queue_buff_error1:
	ion_free(vpe_dev->client, buff->map_info.ion_handle);
	buff->map_info.ion_handle = NULL;
	kzfree(buff);

	return 0;
}

static void msm_vpe_dequeue_buffer_info(struct vpe_device *vpe_dev,
	struct msm_vpe_buffer_map_list_t *buff)
{
	ion_unmap_iommu(vpe_dev->client, buff->map_info.ion_handle,
		vpe_dev->domain_num, 0);
	ion_free(vpe_dev->client, buff->map_info.ion_handle);
	buff->map_info.ion_handle = NULL;

	list_del_init(&buff->entry);
	kzfree(buff);

	return;
}

static unsigned long msm_vpe_fetch_buffer_info(struct vpe_device *vpe_dev,
	struct msm_vpe_buffer_info_t *buffer_info, uint32_t session_id,
	uint32_t stream_id)
{
	unsigned long phy_addr = 0;
	struct msm_vpe_buff_queue_info_t *buff_queue_info;
	uint8_t native_buff = buffer_info->native_buff;

	buff_queue_info = msm_vpe_get_buff_queue_entry(vpe_dev, session_id,
		stream_id);
	if (buff_queue_info == NULL) {
		pr_err("error finding buffer queue entry for sessid:%d strmid:%d\n",
			session_id, stream_id);
		return phy_addr;
	}

	phy_addr = msm_vpe_get_phy_addr(vpe_dev, buff_queue_info,
		buffer_info->index, native_buff);
	if ((phy_addr == 0) && (native_buff)) {
		phy_addr = msm_vpe_queue_buffer_info(vpe_dev, buff_queue_info,
			buffer_info);
	}
	return phy_addr;
}

static int32_t msm_vpe_enqueue_buff_info_list(struct vpe_device *vpe_dev,
	struct msm_vpe_stream_buff_info_t *stream_buff_info)
{
	uint32_t j;
	struct msm_vpe_buff_queue_info_t *buff_queue_info;

	buff_queue_info = msm_vpe_get_buff_queue_entry(vpe_dev,
			(stream_buff_info->identity >> 16) & 0xFFFF,
			stream_buff_info->identity & 0xFFFF);
	if (buff_queue_info == NULL) {
		pr_err("error finding buffer queue entry for sessid:%d strmid:%d\n",
			(stream_buff_info->identity >> 16) & 0xFFFF,
			stream_buff_info->identity & 0xFFFF);
		return -EINVAL;
	}

	for (j = 0; j < stream_buff_info->num_buffs; j++) {
		msm_vpe_queue_buffer_info(vpe_dev, buff_queue_info,
		&stream_buff_info->buffer_info[j]);
	}
	return 0;
}

static int32_t msm_vpe_dequeue_buff_info_list(struct vpe_device *vpe_dev,
	struct msm_vpe_buff_queue_info_t *buff_queue_info)
{
	struct msm_vpe_buffer_map_list_t *buff, *save;
	struct list_head *buff_head;

	buff_head = &buff_queue_info->native_buff_head;
	list_for_each_entry_safe(buff, save, buff_head, entry) {
		msm_vpe_dequeue_buffer_info(vpe_dev, buff);
	}

	buff_head = &buff_queue_info->vb2_buff_head;
	list_for_each_entry_safe(buff, save, buff_head, entry) {
		msm_vpe_dequeue_buffer_info(vpe_dev, buff);
	}

	return 0;
}

static int32_t msm_vpe_add_buff_queue_entry(struct vpe_device *vpe_dev,
	uint16_t session_id, uint16_t stream_id)
{
	uint32_t i;
	struct msm_vpe_buff_queue_info_t *buff_queue_info;

	for (i = 0; i < vpe_dev->num_buffq; i++) {
		if (vpe_dev->buff_queue[i].used == 0) {
			buff_queue_info = &vpe_dev->buff_queue[i];
			buff_queue_info->used = 1;
			buff_queue_info->session_id = session_id;
			buff_queue_info->stream_id = stream_id;
			INIT_LIST_HEAD(&buff_queue_info->vb2_buff_head);
			INIT_LIST_HEAD(&buff_queue_info->native_buff_head);
			return 0;
		}
	}
	pr_err("buffer queue full. error for sessionid: %d streamid: %d\n",
		session_id, stream_id);
	return -EINVAL;
}

static int32_t msm_vpe_free_buff_queue_entry(struct vpe_device *vpe_dev,
					uint32_t session_id, uint32_t stream_id)
{
	struct msm_vpe_buff_queue_info_t *buff_queue_info;

	buff_queue_info = msm_vpe_get_buff_queue_entry(vpe_dev, session_id,
		stream_id);
	if (buff_queue_info == NULL) {
		pr_err("error finding buffer queue entry for sessid:%d strmid:%d\n",
			session_id, stream_id);
		return -EINVAL;
	}

	buff_queue_info->used = 0;
	buff_queue_info->session_id = 0;
	buff_queue_info->stream_id = 0;
	INIT_LIST_HEAD(&buff_queue_info->vb2_buff_head);
	INIT_LIST_HEAD(&buff_queue_info->native_buff_head);
	return 0;
}

static int32_t msm_vpe_create_buff_queue(struct vpe_device *vpe_dev,
					uint32_t num_buffq)
{
	struct msm_vpe_buff_queue_info_t *buff_queue;
	buff_queue = kzalloc(
		sizeof(struct msm_vpe_buff_queue_info_t) * num_buffq,
		GFP_KERNEL);
	if (!buff_queue) {
		pr_err("Buff queue allocation failure\n");
		return -ENOMEM;
	}

	if (vpe_dev->buff_queue) {
		pr_err("Buff queue not empty\n");
		kzfree(buff_queue);
		return -EINVAL;
	} else {
		vpe_dev->buff_queue = buff_queue;
		vpe_dev->num_buffq = num_buffq;
	}
	return 0;
}

static void msm_vpe_delete_buff_queue(struct vpe_device *vpe_dev)
{
	uint32_t i;

	for (i = 0; i < vpe_dev->num_buffq; i++) {
		if (vpe_dev->buff_queue[i].used == 1) {
			pr_err("Queue not free sessionid: %d, streamid: %d\n",
				vpe_dev->buff_queue[i].session_id,
				vpe_dev->buff_queue[i].stream_id);
			msm_vpe_free_buff_queue_entry(vpe_dev,
				vpe_dev->buff_queue[i].session_id,
				vpe_dev->buff_queue[i].stream_id);
		}
	}
	kzfree(vpe_dev->buff_queue);
	vpe_dev->buff_queue = NULL;
	vpe_dev->num_buffq = 0;
	return;
}

void vpe_release_ion_client(struct kref *ref)
{
	struct vpe_device *vpe_dev = container_of(ref,
		struct vpe_device, refcount);
	ion_client_destroy(vpe_dev->client);
}

static int vpe_init_mem(struct vpe_device *vpe_dev)
{
	kref_init(&vpe_dev->refcount);
	kref_get(&vpe_dev->refcount);
	vpe_dev->client = msm_ion_client_create(-1, "vpe");

	if (!vpe_dev->client) {
		pr_err("couldn't create ion client\n");
		return  -ENODEV;
	}

	return 0;
}

static void vpe_deinit_mem(struct vpe_device *vpe_dev)
{
	kref_put(&vpe_dev->refcount, vpe_release_ion_client);
}

static irqreturn_t msm_vpe_irq(int irq_num, void *data)
{
	unsigned long flags;
	uint32_t irq_status;
	struct msm_vpe_tasklet_queue_cmd *queue_cmd;
	struct vpe_device *vpe_dev = (struct vpe_device *) data;

	irq_status = msm_camera_io_r_mb(vpe_dev->base +
					VPE_INTR_STATUS_OFFSET);

	spin_lock_irqsave(&vpe_dev->tasklet_lock, flags);
	queue_cmd = &vpe_dev->tasklet_queue_cmd[vpe_dev->taskletq_idx];
	if (queue_cmd->cmd_used) {
		VPE_DBG("%s: vpe tasklet queue overflow\n", __func__);
		list_del(&queue_cmd->list);
	} else {
		atomic_add(1, &vpe_dev->irq_cnt);
	}
	queue_cmd->irq_status = irq_status;

	queue_cmd->cmd_used = 1;
	vpe_dev->taskletq_idx =
		(vpe_dev->taskletq_idx + 1) % MSM_VPE_TASKLETQ_SIZE;
	list_add_tail(&queue_cmd->list, &vpe_dev->tasklet_q);
	spin_unlock_irqrestore(&vpe_dev->tasklet_lock, flags);

	tasklet_schedule(&vpe_dev->vpe_tasklet);

	msm_camera_io_w_mb(irq_status, vpe_dev->base + VPE_INTR_CLEAR_OFFSET);
	msm_camera_io_w(0, vpe_dev->base + VPE_INTR_ENABLE_OFFSET);
	VPE_DBG("%s: irq_status=0x%x.\n", __func__, irq_status);

	return IRQ_HANDLED;
}

static void msm_vpe_do_tasklet(unsigned long data)
{
	unsigned long flags;
	struct vpe_device *vpe_dev = (struct vpe_device *)data;
	struct msm_vpe_tasklet_queue_cmd *queue_cmd;

	while (atomic_read(&vpe_dev->irq_cnt)) {
		spin_lock_irqsave(&vpe_dev->tasklet_lock, flags);
		queue_cmd = list_first_entry(&vpe_dev->tasklet_q,
					struct msm_vpe_tasklet_queue_cmd, list);
		if (!queue_cmd) {
			atomic_set(&vpe_dev->irq_cnt, 0);
			spin_unlock_irqrestore(&vpe_dev->tasklet_lock, flags);
			return;
		}
		atomic_sub(1, &vpe_dev->irq_cnt);
		list_del(&queue_cmd->list);
		queue_cmd->cmd_used = 0;

		spin_unlock_irqrestore(&vpe_dev->tasklet_lock, flags);

		VPE_DBG("Frame done!!\n");
		msm_vpe_notify_frame_done(vpe_dev);
	}
}

static int vpe_init_hardware(struct vpe_device *vpe_dev)
{
	int rc = 0;

	if (vpe_dev->fs_vpe == NULL) {
		vpe_dev->fs_vpe =
			regulator_get(&vpe_dev->pdev->dev, "vdd");
		if (IS_ERR(vpe_dev->fs_vpe)) {
			pr_err("Regulator vpe vdd get failed %ld\n",
				PTR_ERR(vpe_dev->fs_vpe));
			vpe_dev->fs_vpe = NULL;
			rc = -ENODEV;
			goto fail;
		} else if (regulator_enable(vpe_dev->fs_vpe)) {
			pr_err("Regulator vpe vdd enable failed\n");
			regulator_put(vpe_dev->fs_vpe);
			vpe_dev->fs_vpe = NULL;
			rc = -ENODEV;
			goto fail;
		}
	}

	rc = msm_cam_clk_enable(&vpe_dev->pdev->dev, vpe_clk_info,
				vpe_dev->vpe_clk, ARRAY_SIZE(vpe_clk_info), 1);
	if (rc < 0) {
		rc = -ENODEV;
		pr_err("clk enable failed\n");
		goto disable_and_put_regulator;
	}

	vpe_dev->base = ioremap(vpe_dev->mem->start,
		resource_size(vpe_dev->mem));
	if (!vpe_dev->base) {
		rc = -ENOMEM;
		pr_err("ioremap failed\n");
		goto disable_and_put_regulator;
	}

	if (vpe_dev->state != VPE_STATE_BOOT) {
		rc = request_irq(vpe_dev->irq->start, msm_vpe_irq,
				IRQF_TRIGGER_RISING,
				"vpe", vpe_dev);
		if (rc < 0) {
			pr_err("irq request fail! start=%u\n",
				vpe_dev->irq->start);
			rc = -EBUSY;
			goto unmap_base;
		} else {
			VPE_DBG("Got irq! %d\n", vpe_dev->irq->start);
		}
	} else {
		VPE_DBG("Skip requesting the irq since device is booting\n");
	}
	vpe_dev->buf_mgr_subdev = msm_buf_mngr_get_subdev();

	msm_vpe_create_buff_queue(vpe_dev, MSM_VPE_MAX_BUFF_QUEUE);
	return rc;

unmap_base:
	iounmap(vpe_dev->base);
disable_and_put_regulator:
	regulator_disable(vpe_dev->fs_vpe);
	regulator_put(vpe_dev->fs_vpe);
fail:
	return rc;
}

static int vpe_release_hardware(struct vpe_device *vpe_dev)
{
	if (vpe_dev->state != VPE_STATE_BOOT) {
		free_irq(vpe_dev->irq->start, vpe_dev);
		tasklet_kill(&vpe_dev->vpe_tasklet);
		atomic_set(&vpe_dev->irq_cnt, 0);
	}

	msm_vpe_delete_buff_queue(vpe_dev);
	iounmap(vpe_dev->base);
	msm_cam_clk_enable(&vpe_dev->pdev->dev, vpe_clk_info,
			vpe_dev->vpe_clk, ARRAY_SIZE(vpe_clk_info), 0);
	return 0;
}

static int vpe_open_node(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	uint32_t i;
	struct vpe_device *vpe_dev = v4l2_get_subdevdata(sd);

	mutex_lock(&vpe_dev->mutex);
	if (vpe_dev->vpe_open_cnt == MAX_ACTIVE_VPE_INSTANCE) {
		pr_err("No free VPE instance\n");
		rc = -ENODEV;
		goto err_mutex_unlock;
	}

	for (i = 0; i < MAX_ACTIVE_VPE_INSTANCE; i++) {
		if (vpe_dev->vpe_subscribe_list[i].active == 0) {
			vpe_dev->vpe_subscribe_list[i].active = 1;
			vpe_dev->vpe_subscribe_list[i].vfh = &fh->vfh;
			break;
		}
	}
	if (i == MAX_ACTIVE_VPE_INSTANCE) {
		pr_err("No free instance\n");
		rc = -ENODEV;
		goto err_mutex_unlock;
	}

	VPE_DBG("open %d %p\n", i, &fh->vfh);
	vpe_dev->vpe_open_cnt++;
	if (vpe_dev->vpe_open_cnt == 1) {
		rc = vpe_init_hardware(vpe_dev);
		if (rc < 0) {
			pr_err("%s: Couldn't init vpe hardware\n", __func__);
			vpe_dev->vpe_open_cnt--;
			rc = -ENODEV;
			goto err_fixup_sub_list;
		}
		rc = vpe_init_mem(vpe_dev);
		if (rc < 0) {
			pr_err("%s: Couldn't init mem\n", __func__);
			vpe_dev->vpe_open_cnt--;
			rc = -ENODEV;
			goto err_release_hardware;
		}
		vpe_dev->state = VPE_STATE_IDLE;
	}
	mutex_unlock(&vpe_dev->mutex);

	return rc;

err_release_hardware:
	vpe_release_hardware(vpe_dev);
err_fixup_sub_list:
	for (i = 0; i < MAX_ACTIVE_VPE_INSTANCE; i++) {
		if (vpe_dev->vpe_subscribe_list[i].vfh == &fh->vfh) {
			vpe_dev->vpe_subscribe_list[i].active = 0;
			vpe_dev->vpe_subscribe_list[i].vfh = NULL;
			break;
		}
	}
err_mutex_unlock:
	mutex_unlock(&vpe_dev->mutex);
	return rc;
}

static int vpe_close_node(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	uint32_t i;
	struct vpe_device *vpe_dev = v4l2_get_subdevdata(sd);
	mutex_lock(&vpe_dev->mutex);
	for (i = 0; i < MAX_ACTIVE_VPE_INSTANCE; i++) {
		if (vpe_dev->vpe_subscribe_list[i].vfh == &fh->vfh) {
			vpe_dev->vpe_subscribe_list[i].active = 0;
			vpe_dev->vpe_subscribe_list[i].vfh = NULL;
			break;
		}
	}
	if (i == MAX_ACTIVE_VPE_INSTANCE) {
		pr_err("Invalid close\n");
		mutex_unlock(&vpe_dev->mutex);
		return -ENODEV;
	}

	VPE_DBG("close %d %p\n", i, &fh->vfh);
	vpe_dev->vpe_open_cnt--;
	if (vpe_dev->vpe_open_cnt == 0) {
		vpe_deinit_mem(vpe_dev);
		vpe_release_hardware(vpe_dev);
		vpe_dev->state = VPE_STATE_OFF;
	}
	mutex_unlock(&vpe_dev->mutex);
	return 0;
}

static const struct v4l2_subdev_internal_ops msm_vpe_internal_ops = {
	.open = vpe_open_node,
	.close = vpe_close_node,
};

static int msm_vpe_buffer_ops(struct vpe_device *vpe_dev,
	uint32_t buff_mgr_ops, struct msm_buf_mngr_info *buff_mgr_info)
{
	int rc = -EINVAL;

	rc = v4l2_subdev_call(vpe_dev->buf_mgr_subdev, core, ioctl,
		buff_mgr_ops, buff_mgr_info);
	if (rc < 0)
		pr_err("%s: line %d rc = %d\n", __func__, __LINE__, rc);
	return rc;
}

static int msm_vpe_notify_frame_done(struct vpe_device *vpe_dev)
{
	struct v4l2_event v4l2_evt;
	struct msm_queue_cmd *frame_qcmd;
	struct msm_queue_cmd *event_qcmd;
	struct msm_vpe_frame_info_t *processed_frame;
	struct msm_device_queue *queue = &vpe_dev->processing_q;
	struct msm_buf_mngr_info buff_mgr_info;
	int rc = 0;

	if (queue->len > 0) {
		frame_qcmd = msm_dequeue(queue, list_frame);
		if(frame_qcmd) {
			processed_frame = frame_qcmd->command;
			do_gettimeofday(&(processed_frame->out_time));
			kfree(frame_qcmd);
			event_qcmd = kzalloc(sizeof(struct msm_queue_cmd), GFP_ATOMIC);
			if (!event_qcmd) {
				pr_err("%s: Insufficient memory\n", __func__);
				return -ENOMEM;
			}
			atomic_set(&event_qcmd->on_heap, 1);
			event_qcmd->command = processed_frame;
			VPE_DBG("fid %d\n", processed_frame->frame_id);
			msm_enqueue(&vpe_dev->eventData_q, &event_qcmd->list_eventdata);

			if (!processed_frame->output_buffer_info.processed_divert) {
				memset(&buff_mgr_info, 0 ,
					sizeof(buff_mgr_info));
				buff_mgr_info.session_id =
					((processed_frame->identity >> 16) & 0xFFFF);
				buff_mgr_info.stream_id =
					(processed_frame->identity & 0xFFFF);
				buff_mgr_info.frame_id = processed_frame->frame_id;
				buff_mgr_info.timestamp = processed_frame->timestamp;
				buff_mgr_info.index =
					processed_frame->output_buffer_info.index;
				rc = msm_vpe_buffer_ops(vpe_dev,
						VIDIOC_MSM_BUF_MNGR_BUF_DONE,
						&buff_mgr_info);
				if (rc < 0) {
					pr_err("%s: error doing VIDIOC_MSM_BUF_MNGR_BUF_DONE\n",
						__func__);
					rc = -EINVAL;
				}
			}

			v4l2_evt.id = processed_frame->inst_id;
			v4l2_evt.type = V4L2_EVENT_VPE_FRAME_DONE;
			v4l2_event_queue(vpe_dev->msm_sd.sd.devnode, &v4l2_evt);
		}
		else
			rc = -EFAULT;
	}
	return rc;
}

static void vpe_update_scaler_params(struct vpe_device *vpe_dev,
			struct msm_vpe_frame_strip_info strip_info)
{
	uint32_t out_ROI_width, out_ROI_height;
	uint32_t src_ROI_width, src_ROI_height;

	/*
	* phase_step_x, phase_step_y, phase_init_x and phase_init_y
	* are represented in fixed-point, unsigned 3.29 format
	*/
	uint32_t phase_step_x = 0;
	uint32_t phase_step_y = 0;
	uint32_t phase_init_x = 0;
	uint32_t phase_init_y = 0;

	uint32_t src_roi, src_x, src_y, src_xy, temp;
	uint32_t yscale_filter_sel, xscale_filter_sel;
	uint32_t scale_unit_sel_x, scale_unit_sel_y;
	uint64_t numerator, denominator;

	/*
	 * assumption is both direction need zoom. this can be
	 * improved.
	 */
	temp = msm_camera_io_r(vpe_dev->base + VPE_OP_MODE_OFFSET) | 0x3;
	msm_camera_io_w(temp, vpe_dev->base + VPE_OP_MODE_OFFSET);

	src_ROI_width  = strip_info.src_w;
	src_ROI_height = strip_info.src_h;
	out_ROI_width  = strip_info.dst_w;
	out_ROI_height = strip_info.dst_h;

	VPE_DBG("src w = %u, h=%u, dst w = %u, h =%u.\n",
		src_ROI_width, src_ROI_height, out_ROI_width,
		out_ROI_height);
	src_roi = (src_ROI_height << 16) + src_ROI_width;

	msm_camera_io_w(src_roi, vpe_dev->base + VPE_SRC_SIZE_OFFSET);

	src_x = strip_info.src_x;
	src_y = strip_info.src_y;

	VPE_DBG("src_x = %d, src_y=%d.\n", src_x, src_y);

	src_xy = src_y*(1<<16) + src_x;
	msm_camera_io_w(src_xy, vpe_dev->base +
			VPE_SRC_XY_OFFSET);
	VPE_DBG("src_xy = 0x%x, src_roi=0x%x.\n", src_xy, src_roi);

	/* decide whether to use FIR or M/N for scaling */
	if ((out_ROI_width == 1 && src_ROI_width < 4) ||
		(src_ROI_width < 4 * out_ROI_width - 3))
		scale_unit_sel_x = 0;/* use FIR scalar */
	else
		scale_unit_sel_x = 1;/* use M/N scalar */

	if ((out_ROI_height == 1 && src_ROI_height < 4) ||
		(src_ROI_height < 4 * out_ROI_height - 3))
		scale_unit_sel_y = 0;/* use FIR scalar */
	else
		scale_unit_sel_y = 1;/* use M/N scalar */

	/* calculate phase step for the x direction */

	/*
	 * if destination is only 1 pixel wide, the value of
	 * phase_step_x is unimportant. Assigning phase_step_x to src
	 * ROI width as an arbitrary value.
	 */
	if (out_ROI_width == 1)
		phase_step_x = (uint32_t) ((src_ROI_width) <<
						SCALER_PHASE_BITS);

		/* if using FIR scalar */
	else if (scale_unit_sel_x == 0) {

		/*
		 * Calculate the quotient ( src_ROI_width - 1 ) (
		 * out_ROI_width - 1) with u3.29 precision. Quotient
		 * is rounded up to the larger 29th decimal point
		 */
		numerator = (uint64_t)(src_ROI_width - 1) <<
			SCALER_PHASE_BITS;
		/*
		 * never equals to 0 because of the "(out_ROI_width ==
		 * 1 )"
		 */
		denominator = (uint64_t)(out_ROI_width - 1);
		/*
		 * divide and round up to the larger 29th decimal
		 * point.
		 */
		phase_step_x = (uint32_t) vpe_do_div((numerator +
					denominator - 1), denominator);
	} else if (scale_unit_sel_x == 1) { /* if M/N scalar */
		/*
		 * Calculate the quotient ( src_ROI_width ) / (
		 * out_ROI_width) with u3.29 precision. Quotient is
		 * rounded down to the smaller 29th decimal point.
		 */
		numerator = (uint64_t)(src_ROI_width) <<
			SCALER_PHASE_BITS;
		denominator = (uint64_t)(out_ROI_width);
		phase_step_x =
			(uint32_t) vpe_do_div(numerator, denominator);
	}
	/* calculate phase step for the y direction */

	/*
	 * if destination is only 1 pixel wide, the value of
	 * phase_step_x is unimportant. Assigning phase_step_x to src
	 * ROI width as an arbitrary value.
	 */
	if (out_ROI_height == 1)
		phase_step_y =
		(uint32_t) ((src_ROI_height) << SCALER_PHASE_BITS);

	/* if FIR scalar */
	else if (scale_unit_sel_y == 0) {
		/*
		 * Calculate the quotient ( src_ROI_height - 1 ) / (
		 * out_ROI_height - 1) with u3.29 precision. Quotient
		 * is rounded up to the larger 29th decimal point.
		 */
		numerator = (uint64_t)(src_ROI_height - 1) <<
			SCALER_PHASE_BITS;
		/*
		 * never equals to 0 because of the " ( out_ROI_height
		 * == 1 )" case
		 */
		denominator = (uint64_t)(out_ROI_height - 1);
		/*
		 * Quotient is rounded up to the larger 29th decimal
		 * point.
		 */
		phase_step_y =
		(uint32_t) vpe_do_div(
			(numerator + denominator - 1), denominator);
	} else if (scale_unit_sel_y == 1) { /* if M/N scalar */
		/*
		 * Calculate the quotient ( src_ROI_height ) (
		 * out_ROI_height) with u3.29 precision. Quotient is
		 * rounded down to the smaller 29th decimal point.
		 */
		numerator = (uint64_t)(src_ROI_height) <<
			SCALER_PHASE_BITS;
		denominator = (uint64_t)(out_ROI_height);
		phase_step_y = (uint32_t) vpe_do_div(
			numerator, denominator);
	}

	/* decide which set of FIR coefficients to use */
	if (phase_step_x > HAL_MDP_PHASE_STEP_2P50)
		xscale_filter_sel = 0;
	else if (phase_step_x > HAL_MDP_PHASE_STEP_1P66)
		xscale_filter_sel = 1;
	else if (phase_step_x > HAL_MDP_PHASE_STEP_1P25)
		xscale_filter_sel = 2;
	else
		xscale_filter_sel = 3;

	if (phase_step_y > HAL_MDP_PHASE_STEP_2P50)
		yscale_filter_sel = 0;
	else if (phase_step_y > HAL_MDP_PHASE_STEP_1P66)
		yscale_filter_sel = 1;
	else if (phase_step_y > HAL_MDP_PHASE_STEP_1P25)
		yscale_filter_sel = 2;
	else
		yscale_filter_sel = 3;

	/* calculate phase init for the x direction */

	/* if using FIR scalar */
	if (scale_unit_sel_x == 0) {
		if (out_ROI_width == 1)
			phase_init_x =
				(uint32_t) ((src_ROI_width - 1) <<
							SCALER_PHASE_BITS);
		else
			phase_init_x = 0;
	} else if (scale_unit_sel_x == 1) /* M over N scalar  */
		phase_init_x = 0;

	/*
	 * calculate phase init for the y direction if using FIR
	 * scalar
	 */
	if (scale_unit_sel_y == 0) {
		if (out_ROI_height == 1)
			phase_init_y =
			(uint32_t) ((src_ROI_height -
						1) << SCALER_PHASE_BITS);
		else
			phase_init_y = 0;
	} else if (scale_unit_sel_y == 1) /* M over N scalar   */
		phase_init_y = 0;

	strip_info.phase_step_x = phase_step_x;
	strip_info.phase_step_y = phase_step_y;
	strip_info.phase_init_x = phase_init_x;
	strip_info.phase_init_y = phase_init_y;
	VPE_DBG("phase step x = %d, step y = %d.\n",
		 strip_info.phase_step_x, strip_info.phase_step_y);
	VPE_DBG("phase init x = %d, init y = %d.\n",
		 strip_info.phase_init_x, strip_info.phase_init_y);

	msm_camera_io_w(strip_info.phase_step_x, vpe_dev->base +
			VPE_SCALE_PHASEX_STEP_OFFSET);
	msm_camera_io_w(strip_info.phase_step_y, vpe_dev->base +
			VPE_SCALE_PHASEY_STEP_OFFSET);

	msm_camera_io_w(strip_info.phase_init_x, vpe_dev->base +
			VPE_SCALE_PHASEX_INIT_OFFSET);
	msm_camera_io_w(strip_info.phase_init_y, vpe_dev->base +
			VPE_SCALE_PHASEY_INIT_OFFSET);
}

static void vpe_program_buffer_addresses(
	struct vpe_device *vpe_dev,
	unsigned long srcP0,
	unsigned long srcP1,
	unsigned long outP0,
	unsigned long outP1)
{
	VPE_DBG("%s VPE Configured with:\n"
		"Src %x, %x Dest %x, %x",
		__func__, (uint32_t)srcP0, (uint32_t)srcP1,
		(uint32_t)outP0, (uint32_t)outP1);

	msm_camera_io_w(srcP0, vpe_dev->base + VPE_SRCP0_ADDR_OFFSET);
	msm_camera_io_w(srcP1, vpe_dev->base + VPE_SRCP1_ADDR_OFFSET);
	msm_camera_io_w(outP0, vpe_dev->base + VPE_OUTP0_ADDR_OFFSET);
	msm_camera_io_w(outP1, vpe_dev->base + VPE_OUTP1_ADDR_OFFSET);
}

static int vpe_start(struct vpe_device *vpe_dev)
{
	/*  enable the frame irq, bit 0 = Display list 0 ROI done */
	msm_camera_io_w_mb(1, vpe_dev->base + VPE_INTR_ENABLE_OFFSET);
	msm_camera_io_dump(vpe_dev->base, 0x120);
	msm_camera_io_dump(vpe_dev->base + 0x00400, 0x18);
	msm_camera_io_dump(vpe_dev->base + 0x10000, 0x250);
	msm_camera_io_dump(vpe_dev->base + 0x30000, 0x20);
	msm_camera_io_dump(vpe_dev->base + 0x50000, 0x30);
	msm_camera_io_dump(vpe_dev->base + 0x50400, 0x10);

	/*
	 * This triggers the operation. When the VPE is done,
	 * msm_vpe_irq will fire.
	 */
	msm_camera_io_w_mb(1, vpe_dev->base + VPE_DL0_START_OFFSET);
	return 0;
}

static void vpe_config_axi_default(struct vpe_device *vpe_dev)
{
	msm_camera_io_w(0x25, vpe_dev->base + VPE_AXI_ARB_2_OFFSET);
}

static int vpe_reset(struct vpe_device *vpe_dev)
{
	uint32_t vpe_version;
	uint32_t rc = 0;

	vpe_version = msm_camera_io_r(
			vpe_dev->base + VPE_HW_VERSION_OFFSET);
	VPE_DBG("vpe_version = 0x%x\n", vpe_version);
	/* disable all interrupts.*/
	msm_camera_io_w(0, vpe_dev->base + VPE_INTR_ENABLE_OFFSET);
	/* clear all pending interrupts*/
	msm_camera_io_w(0x1fffff, vpe_dev->base + VPE_INTR_CLEAR_OFFSET);
	/* write sw_reset to reset the core. */
	msm_camera_io_w(0x10, vpe_dev->base + VPE_SW_RESET_OFFSET);
	/* then poll the reset bit, it should be self-cleared. */
	while (1) {
		rc = msm_camera_io_r(vpe_dev->base + VPE_SW_RESET_OFFSET) \
			& 0x10;
		if (rc == 0)
			break;
		cpu_relax();
	}
	/*
	 * at this point, hardware is reset. Then pogram to default
	 * values.
	 */
	msm_camera_io_w(VPE_AXI_RD_ARB_CONFIG_VALUE,
			vpe_dev->base + VPE_AXI_RD_ARB_CONFIG_OFFSET);

	msm_camera_io_w(VPE_CGC_ENABLE_VALUE,
			vpe_dev->base + VPE_CGC_EN_OFFSET);
	msm_camera_io_w(1, vpe_dev->base + VPE_CMD_MODE_OFFSET);
	msm_camera_io_w(VPE_DEFAULT_OP_MODE_VALUE,
			vpe_dev->base + VPE_OP_MODE_OFFSET);
	msm_camera_io_w(VPE_DEFAULT_SCALE_CONFIG,
			vpe_dev->base + VPE_SCALE_CONFIG_OFFSET);
	vpe_config_axi_default(vpe_dev);
	return rc;
}

static void vpe_update_scale_coef(struct vpe_device *vpe_dev, uint32_t *p)
{
	uint32_t i, offset;
	offset = *p;
	for (i = offset; i < (VPE_SCALE_COEFF_NUM + offset); i++) {
		VPE_DBG("Setting scale table %d\n", i);
		msm_camera_io_w(*(++p),
			vpe_dev->base + VPE_SCALE_COEFF_LSBn(i));
		msm_camera_io_w(*(++p),
			vpe_dev->base + VPE_SCALE_COEFF_MSBn(i));
	}
}

static void vpe_input_plane_config(struct vpe_device *vpe_dev, uint32_t *p)
{
	msm_camera_io_w(*p, vpe_dev->base + VPE_SRC_FORMAT_OFFSET);
	msm_camera_io_w(*(++p),
		vpe_dev->base + VPE_SRC_UNPACK_PATTERN1_OFFSET);
	msm_camera_io_w(*(++p), vpe_dev->base + VPE_SRC_IMAGE_SIZE_OFFSET);
	msm_camera_io_w(*(++p), vpe_dev->base + VPE_SRC_YSTRIDE1_OFFSET);
	msm_camera_io_w(*(++p), vpe_dev->base + VPE_SRC_SIZE_OFFSET);
	msm_camera_io_w(*(++p), vpe_dev->base + VPE_SRC_XY_OFFSET);
}

static void vpe_output_plane_config(struct vpe_device *vpe_dev, uint32_t *p)
{
	msm_camera_io_w(*p, vpe_dev->base + VPE_OUT_FORMAT_OFFSET);
	msm_camera_io_w(*(++p),
		vpe_dev->base + VPE_OUT_PACK_PATTERN1_OFFSET);
	msm_camera_io_w(*(++p), vpe_dev->base + VPE_OUT_YSTRIDE1_OFFSET);
	msm_camera_io_w(*(++p), vpe_dev->base + VPE_OUT_SIZE_OFFSET);
	msm_camera_io_w(*(++p), vpe_dev->base + VPE_OUT_XY_OFFSET);
}

static void vpe_operation_config(struct vpe_device *vpe_dev, uint32_t *p)
{
	msm_camera_io_w(*p, vpe_dev->base + VPE_OP_MODE_OFFSET);
}

/**
 * msm_vpe_transaction_setup() - send setup for one frame to VPE
 * @vpe_dev:	vpe device
 * @data:	packed setup commands
 *
 * See msm_vpe.h for the expected format of `data'
 */
static void msm_vpe_transaction_setup(struct vpe_device *vpe_dev, void *data)
{
	int i;
	void *iter = data;

	vpe_mem_dump("vpe_transaction", data, VPE_TRANSACTION_SETUP_CONFIG_LEN);

	for (i = 0; i < VPE_NUM_SCALER_TABLES; ++i) {
		vpe_update_scale_coef(vpe_dev, (uint32_t *)iter);
		iter += VPE_SCALER_CONFIG_LEN;
	}
	vpe_input_plane_config(vpe_dev, (uint32_t *)iter);
	iter += VPE_INPUT_PLANE_CFG_LEN;
	vpe_output_plane_config(vpe_dev, (uint32_t *)iter);
	iter += VPE_OUTPUT_PLANE_CFG_LEN;
	vpe_operation_config(vpe_dev, (uint32_t *)iter);
}

static int msm_vpe_send_frame_to_hardware(struct vpe_device *vpe_dev,
	struct msm_queue_cmd *frame_qcmd)
{
	struct msm_vpe_frame_info_t *process_frame;

	if (vpe_dev->processing_q.len < MAX_VPE_PROCESSING_FRAME) {
		process_frame = frame_qcmd->command;
		msm_enqueue(&vpe_dev->processing_q,
					&frame_qcmd->list_frame);

		vpe_update_scaler_params(vpe_dev, process_frame->strip_info);
		vpe_program_buffer_addresses(
			vpe_dev,
			process_frame->src_phyaddr,
			process_frame->src_phyaddr
			+ process_frame->src_chroma_plane_offset,
			process_frame->dest_phyaddr,
			process_frame->dest_phyaddr
			+ process_frame->dest_chroma_plane_offset);
		vpe_start(vpe_dev);
		do_gettimeofday(&(process_frame->in_time));
	}
	return 0;
}

static int msm_vpe_cfg(struct vpe_device *vpe_dev,
	struct msm_camera_v4l2_ioctl_t *ioctl_ptr)
{
	int rc = 0;
	struct msm_queue_cmd *frame_qcmd = NULL;
	struct msm_vpe_frame_info_t *new_frame =
		kzalloc(sizeof(struct msm_vpe_frame_info_t), GFP_KERNEL);
	unsigned long in_phyaddr, out_phyaddr;
	struct msm_buf_mngr_info buff_mgr_info;

	if (!new_frame) {
		pr_err("Insufficient memory. return\n");
		return -ENOMEM;
	}

	rc = copy_from_user(new_frame, (void __user *)ioctl_ptr->ioctl_ptr,
			sizeof(struct msm_vpe_frame_info_t));
	if (rc) {
		pr_err("%s:%d copy from user\n", __func__, __LINE__);
		rc = -EINVAL;
		goto err_free_new_frame;
	}

	in_phyaddr = msm_vpe_fetch_buffer_info(vpe_dev,
		&new_frame->input_buffer_info,
		((new_frame->identity >> 16) & 0xFFFF),
		(new_frame->identity & 0xFFFF));
	if (!in_phyaddr) {
		pr_err("error gettting input physical address\n");
		rc = -EINVAL;
		goto err_free_new_frame;
	}

	memset(&new_frame->output_buffer_info, 0,
		sizeof(struct msm_vpe_buffer_info_t));
	memset(&buff_mgr_info, 0, sizeof(struct msm_buf_mngr_info));
	buff_mgr_info.session_id = ((new_frame->identity >> 16) & 0xFFFF);
	buff_mgr_info.stream_id = (new_frame->identity & 0xFFFF);
	rc = msm_vpe_buffer_ops(vpe_dev, VIDIOC_MSM_BUF_MNGR_GET_BUF,
				&buff_mgr_info);
	if (rc < 0) {
		pr_err("error getting buffer\n");
		rc = -EINVAL;
		goto err_free_new_frame;
	}

	new_frame->output_buffer_info.index = buff_mgr_info.index;
	out_phyaddr = msm_vpe_fetch_buffer_info(vpe_dev,
		&new_frame->output_buffer_info,
		((new_frame->identity >> 16) & 0xFFFF),
		(new_frame->identity & 0xFFFF));
	if (!out_phyaddr) {
		pr_err("error gettting output physical address\n");
		rc = -EINVAL;
		goto err_put_buf;
	}

	new_frame->src_phyaddr = in_phyaddr;
	new_frame->dest_phyaddr = out_phyaddr;

	frame_qcmd = kzalloc(sizeof(struct msm_queue_cmd), GFP_KERNEL);
	if (!frame_qcmd) {
		pr_err("Insufficient memory. return\n");
		rc = -ENOMEM;
		goto err_put_buf;
	}

	atomic_set(&frame_qcmd->on_heap, 1);
	frame_qcmd->command = new_frame;
	rc = msm_vpe_send_frame_to_hardware(vpe_dev, frame_qcmd);
	if (rc < 0) {
		pr_err("error cannot send frame to hardware\n");
		rc = -EINVAL;
		goto err_free_frame_qcmd;
	}

	return rc;

err_free_frame_qcmd:
	kfree(frame_qcmd);
err_put_buf:
	msm_vpe_buffer_ops(vpe_dev, VIDIOC_MSM_BUF_MNGR_PUT_BUF,
		&buff_mgr_info);
err_free_new_frame:
	kfree(new_frame);
	return rc;
}

static long msm_vpe_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	struct vpe_device *vpe_dev = v4l2_get_subdevdata(sd);
	struct msm_camera_v4l2_ioctl_t *ioctl_ptr = arg;
	int rc = 0;

	mutex_lock(&vpe_dev->mutex);
	switch (cmd) {
	case VIDIOC_MSM_VPE_TRANSACTION_SETUP: {
		struct msm_vpe_transaction_setup_cfg *cfg;
		VPE_DBG("VIDIOC_MSM_VPE_TRANSACTION_SETUP\n");
		if (sizeof(*cfg) != ioctl_ptr->len) {
			pr_err("%s: size mismatch cmd=%d, len=%d, expected=%d",
				__func__, cmd, ioctl_ptr->len,
				sizeof(*cfg));
			rc = -EINVAL;
			break;
		}

		cfg = kzalloc(ioctl_ptr->len, GFP_KERNEL);
		if (!cfg) {
			pr_err("%s:%d: malloc error\n", __func__, __LINE__);
			mutex_unlock(&vpe_dev->mutex);
			return -EINVAL;
		}

		rc = copy_from_user(cfg, (void __user *)ioctl_ptr->ioctl_ptr,
				ioctl_ptr->len);
		if (rc) {
			pr_err("%s:%d copy from user\n", __func__, __LINE__);
			kfree(cfg);
			break;
		}

		msm_vpe_transaction_setup(vpe_dev, (void *)cfg);
		kfree(cfg);
		break;
	}
	case VIDIOC_MSM_VPE_CFG: {
		VPE_DBG("VIDIOC_MSM_VPE_CFG\n");
		rc = msm_vpe_cfg(vpe_dev, ioctl_ptr);
		break;
	}
	case VIDIOC_MSM_VPE_ENQUEUE_STREAM_BUFF_INFO: {
		struct msm_vpe_stream_buff_info_t *u_stream_buff_info;
		struct msm_vpe_stream_buff_info_t k_stream_buff_info;

		VPE_DBG("VIDIOC_MSM_VPE_ENQUEUE_STREAM_BUFF_INFO\n");

		if (sizeof(struct msm_vpe_stream_buff_info_t) !=
			ioctl_ptr->len) {
			pr_err("%s:%d: invalid length\n", __func__, __LINE__);
			mutex_unlock(&vpe_dev->mutex);
			return -EINVAL;
		}

		u_stream_buff_info = kzalloc(ioctl_ptr->len, GFP_KERNEL);
		if (!u_stream_buff_info) {
			pr_err("%s:%d: malloc error\n", __func__, __LINE__);
			mutex_unlock(&vpe_dev->mutex);
			return -EINVAL;
		}

		rc = (copy_from_user(u_stream_buff_info,
				(void __user *)ioctl_ptr->ioctl_ptr,
				ioctl_ptr->len) ? -EFAULT : 0);
		if (rc) {
			pr_err("%s:%d copy from user\n", __func__, __LINE__);
			kfree(u_stream_buff_info);
			mutex_unlock(&vpe_dev->mutex);
			return -EINVAL;
		}

		if ((u_stream_buff_info->num_buffs == 0) ||
			(u_stream_buff_info->num_buffs >
				MSM_CAMERA_MAX_STREAM_BUF)) {
			pr_err("%s:%d: Invalid number of buffers\n", __func__,
				__LINE__);
			kfree(u_stream_buff_info);
			mutex_unlock(&vpe_dev->mutex);
			return -EINVAL;
		}
		k_stream_buff_info.num_buffs = u_stream_buff_info->num_buffs;
		k_stream_buff_info.identity = u_stream_buff_info->identity;
		k_stream_buff_info.buffer_info =
			kzalloc(k_stream_buff_info.num_buffs *
			sizeof(struct msm_vpe_buffer_info_t), GFP_KERNEL);
		if (!k_stream_buff_info.buffer_info) {
			pr_err("%s:%d: malloc error\n", __func__, __LINE__);
			kfree(u_stream_buff_info);
			mutex_unlock(&vpe_dev->mutex);
			return -EINVAL;
		}

		rc = (copy_from_user(k_stream_buff_info.buffer_info,
				(void __user *)u_stream_buff_info->buffer_info,
				k_stream_buff_info.num_buffs *
				sizeof(struct msm_vpe_buffer_info_t)) ?
				-EFAULT : 0);
		if (rc) {
			pr_err("%s:%d copy from user\n", __func__, __LINE__);
			kfree(k_stream_buff_info.buffer_info);
			kfree(u_stream_buff_info);
			mutex_unlock(&vpe_dev->mutex);
			return -EINVAL;
		}

		rc = msm_vpe_add_buff_queue_entry(vpe_dev,
			((k_stream_buff_info.identity >> 16) & 0xFFFF),
			(k_stream_buff_info.identity & 0xFFFF));
		if (!rc)
			rc = msm_vpe_enqueue_buff_info_list(vpe_dev,
				&k_stream_buff_info);

		kfree(k_stream_buff_info.buffer_info);
		kfree(u_stream_buff_info);
		break;
	}
	case VIDIOC_MSM_VPE_DEQUEUE_STREAM_BUFF_INFO: {
		uint32_t identity;
		struct msm_vpe_buff_queue_info_t *buff_queue_info;

		VPE_DBG("VIDIOC_MSM_VPE_DEQUEUE_STREAM_BUFF_INFO\n");
		if (ioctl_ptr->len != sizeof(uint32_t)) {
			pr_err("%s:%d Invalid len\n", __func__, __LINE__);
			mutex_unlock(&vpe_dev->mutex);
			return -EINVAL;
		}

		rc = (copy_from_user(&identity,
				(void __user *)ioctl_ptr->ioctl_ptr,
				ioctl_ptr->len) ? -EFAULT : 0);
		if (rc) {
			pr_err("%s:%d copy from user\n", __func__, __LINE__);
			mutex_unlock(&vpe_dev->mutex);
			return -EINVAL;
		}

		buff_queue_info = msm_vpe_get_buff_queue_entry(vpe_dev,
			((identity >> 16) & 0xFFFF), (identity & 0xFFFF));
		if (buff_queue_info == NULL) {
			pr_err("error finding buffer queue entry for identity:%d\n",
				identity);
			mutex_unlock(&vpe_dev->mutex);
			return -EINVAL;
		}

		msm_vpe_dequeue_buff_info_list(vpe_dev, buff_queue_info);
		rc = msm_vpe_free_buff_queue_entry(vpe_dev,
			buff_queue_info->session_id,
			buff_queue_info->stream_id);
		break;
	}
	case VIDIOC_MSM_VPE_GET_EVENTPAYLOAD: {
		struct msm_device_queue *queue = &vpe_dev->eventData_q;
		struct msm_queue_cmd *event_qcmd;
		struct msm_vpe_frame_info_t *process_frame;
		VPE_DBG("VIDIOC_MSM_VPE_GET_EVENTPAYLOAD\n");
		event_qcmd = msm_dequeue(queue, list_eventdata);
		if (NULL == event_qcmd)
			break;
		process_frame = event_qcmd->command;
		VPE_DBG("fid %d\n", process_frame->frame_id);
		if (copy_to_user((void __user *)ioctl_ptr->ioctl_ptr,
				process_frame,
				sizeof(struct msm_vpe_frame_info_t))) {
					mutex_unlock(&vpe_dev->mutex);
					kfree(process_frame);
					kfree(event_qcmd);
					return -EINVAL;
		}

		kfree(process_frame);
		kfree(event_qcmd);
		break;
	}
	}
	mutex_unlock(&vpe_dev->mutex);
	return rc;
}

static int msm_vpe_subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
				struct v4l2_event_subscription *sub)
{
	return v4l2_event_subscribe(fh, sub, MAX_VPE_V4l2_EVENTS);
}

static int msm_vpe_unsubscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
				struct v4l2_event_subscription *sub)
{
	return v4l2_event_unsubscribe(fh, sub);
}

static struct v4l2_subdev_core_ops msm_vpe_subdev_core_ops = {
	.ioctl = msm_vpe_subdev_ioctl,
	.subscribe_event = msm_vpe_subscribe_event,
	.unsubscribe_event = msm_vpe_unsubscribe_event,
};

static const struct v4l2_subdev_ops msm_vpe_subdev_ops = {
	.core = &msm_vpe_subdev_core_ops,
};

static struct v4l2_file_operations msm_vpe_v4l2_subdev_fops;

static long msm_vpe_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);
	struct v4l2_fh *vfh = file->private_data;

	switch (cmd) {
	case VIDIOC_DQEVENT:
		if (!(sd->flags & V4L2_SUBDEV_FL_HAS_EVENTS))
			return -ENOIOCTLCMD;

		return v4l2_event_dequeue(vfh, arg, file->f_flags & O_NONBLOCK);

	case VIDIOC_SUBSCRIBE_EVENT:
		return v4l2_subdev_call(sd, core, subscribe_event, vfh, arg);

	case VIDIOC_UNSUBSCRIBE_EVENT:
		return v4l2_subdev_call(sd, core, unsubscribe_event, vfh, arg);
	case VIDIOC_MSM_VPE_GET_INST_INFO: {
		uint32_t i;
		struct vpe_device *vpe_dev = v4l2_get_subdevdata(sd);
		struct msm_camera_v4l2_ioctl_t *ioctl_ptr = arg;
		struct msm_vpe_frame_info_t inst_info;
		memset(&inst_info, 0, sizeof(struct msm_vpe_frame_info_t));
		for (i = 0; i < MAX_ACTIVE_VPE_INSTANCE; i++) {
			if (vpe_dev->vpe_subscribe_list[i].vfh == vfh) {
				inst_info.inst_id = i;
				break;
			}
		}
		if (copy_to_user(
				(void __user *)ioctl_ptr->ioctl_ptr, &inst_info,
				sizeof(struct msm_vpe_frame_info_t))) {
			return -EINVAL;
		}
	}
	default:
		return v4l2_subdev_call(sd, core, ioctl, cmd, arg);
	}

	return 0;
}

static long msm_vpe_subdev_fops_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_vpe_subdev_do_ioctl);
}

static int vpe_register_domain(void)
{
	struct msm_iova_partition vpe_iommu_partition = {
		/* TODO: verify that these are correct? */
		.start = SZ_128K,
		.size = SZ_2G - SZ_128K,
	};
	struct msm_iova_layout vpe_iommu_layout = {
		.partitions = &vpe_iommu_partition,
		.npartitions = 1,
		.client_name = "camera_vpe",
		.domain_flags = 0,
	};

	return msm_register_domain(&vpe_iommu_layout);
}

static int __devinit vpe_probe(struct platform_device *pdev)
{
	struct vpe_device *vpe_dev;
	int rc = 0;

	vpe_dev = kzalloc(sizeof(struct vpe_device), GFP_KERNEL);
	if (!vpe_dev) {
		pr_err("not enough memory\n");
		return -ENOMEM;
	}

	vpe_dev->vpe_clk = kzalloc(sizeof(struct clk *) *
				ARRAY_SIZE(vpe_clk_info), GFP_KERNEL);
	if (!vpe_dev->vpe_clk) {
		pr_err("not enough memory\n");
		rc = -ENOMEM;
		goto err_free_vpe_dev;
	}

	v4l2_subdev_init(&vpe_dev->msm_sd.sd, &msm_vpe_subdev_ops);
	vpe_dev->msm_sd.sd.internal_ops = &msm_vpe_internal_ops;
	snprintf(vpe_dev->msm_sd.sd.name, ARRAY_SIZE(vpe_dev->msm_sd.sd.name),
		"vpe");
	vpe_dev->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	vpe_dev->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_EVENTS;
	v4l2_set_subdevdata(&vpe_dev->msm_sd.sd, vpe_dev);
	platform_set_drvdata(pdev, &vpe_dev->msm_sd.sd);
	mutex_init(&vpe_dev->mutex);
	spin_lock_init(&vpe_dev->tasklet_lock);

	vpe_dev->pdev = pdev;

	vpe_dev->mem = platform_get_resource_byname(pdev,
						IORESOURCE_MEM, "vpe");
	if (!vpe_dev->mem) {
		pr_err("no mem resource?\n");
		rc = -ENODEV;
		goto err_free_vpe_clk;
	}

	vpe_dev->irq = platform_get_resource_byname(pdev,
						IORESOURCE_IRQ, "vpe");
	if (!vpe_dev->irq) {
		pr_err("%s: no irq resource?\n", __func__);
		rc = -ENODEV;
		goto err_release_mem;
	}

	vpe_dev->domain_num = vpe_register_domain();
	if (vpe_dev->domain_num < 0) {
		pr_err("%s: could not register domain\n", __func__);
		rc = -ENODEV;
		goto err_release_mem;
	}

	vpe_dev->domain =
		msm_get_iommu_domain(vpe_dev->domain_num);
	if (!vpe_dev->domain) {
		pr_err("%s: cannot find domain\n", __func__);
		rc = -ENODEV;
		goto err_release_mem;
	}

	vpe_dev->iommu_ctx_src = msm_iommu_get_ctx("vpe_src");
	vpe_dev->iommu_ctx_dst = msm_iommu_get_ctx("vpe_dst");
	if (!vpe_dev->iommu_ctx_src || !vpe_dev->iommu_ctx_dst) {
		pr_err("%s: cannot get iommu_ctx\n", __func__);
		rc = -ENODEV;
		goto err_release_mem;
	}

	media_entity_init(&vpe_dev->msm_sd.sd.entity, 0, NULL, 0);
	vpe_dev->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	vpe_dev->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_VPE;
	vpe_dev->msm_sd.sd.entity.name = pdev->name;
	msm_sd_register(&vpe_dev->msm_sd);
	msm_vpe_v4l2_subdev_fops.owner = v4l2_subdev_fops.owner;
	msm_vpe_v4l2_subdev_fops.open = v4l2_subdev_fops.open;
	msm_vpe_v4l2_subdev_fops.unlocked_ioctl = msm_vpe_subdev_fops_ioctl;
	msm_vpe_v4l2_subdev_fops.release = v4l2_subdev_fops.release;
	msm_vpe_v4l2_subdev_fops.poll = v4l2_subdev_fops.poll;

	vpe_dev->msm_sd.sd.devnode->fops = &msm_vpe_v4l2_subdev_fops;
	vpe_dev->msm_sd.sd.entity.revision = vpe_dev->msm_sd.sd.devnode->num;
	vpe_dev->state = VPE_STATE_BOOT;
	rc = vpe_init_hardware(vpe_dev);
	if (rc < 0) {
		pr_err("%s: Couldn't init vpe hardware\n", __func__);
		rc = -ENODEV;
		goto err_unregister_sd;
	}
	vpe_reset(vpe_dev);
	vpe_release_hardware(vpe_dev);
	vpe_dev->state = VPE_STATE_OFF;

	rc = iommu_attach_device(vpe_dev->domain, vpe_dev->iommu_ctx_src);
	if (rc < 0) {
		pr_err("Couldn't attach to vpe_src context bank\n");
		rc = -ENODEV;
		goto err_unregister_sd;
	}
	rc = iommu_attach_device(vpe_dev->domain, vpe_dev->iommu_ctx_dst);
	if (rc < 0) {
		pr_err("Couldn't attach to vpe_dst context bank\n");
		rc = -ENODEV;
		goto err_detach_src;
	}

	vpe_dev->state = VPE_STATE_OFF;

	msm_queue_init(&vpe_dev->eventData_q, "vpe-eventdata");
	msm_queue_init(&vpe_dev->processing_q, "vpe-frame");
	INIT_LIST_HEAD(&vpe_dev->tasklet_q);
	tasklet_init(&vpe_dev->vpe_tasklet, msm_vpe_do_tasklet,
		(unsigned long)vpe_dev);
	vpe_dev->vpe_open_cnt = 0;

	return rc;

err_detach_src:
	iommu_detach_device(vpe_dev->domain, vpe_dev->iommu_ctx_src);
err_unregister_sd:
	msm_sd_unregister(&vpe_dev->msm_sd);
err_release_mem:
	release_mem_region(vpe_dev->mem->start, resource_size(vpe_dev->mem));
err_free_vpe_clk:
	kfree(vpe_dev->vpe_clk);
err_free_vpe_dev:
	kfree(vpe_dev);
	return rc;
}

static int vpe_device_remove(struct platform_device *dev)
{
	struct v4l2_subdev *sd = platform_get_drvdata(dev);
	struct vpe_device  *vpe_dev;
	if (!sd) {
		pr_err("%s: Subdevice is NULL\n", __func__);
		return 0;
	}

	vpe_dev = (struct vpe_device *)v4l2_get_subdevdata(sd);
	if (!vpe_dev) {
		pr_err("%s: vpe device is NULL\n", __func__);
		return 0;
	}

	iommu_detach_device(vpe_dev->domain, vpe_dev->iommu_ctx_dst);
	iommu_detach_device(vpe_dev->domain, vpe_dev->iommu_ctx_src);
	msm_sd_unregister(&vpe_dev->msm_sd);
	release_mem_region(vpe_dev->mem->start, resource_size(vpe_dev->mem));
	mutex_destroy(&vpe_dev->mutex);
	kfree(vpe_dev);
	return 0;
}

static struct platform_driver vpe_driver = {
	.probe = vpe_probe,
	.remove = __devexit_p(vpe_device_remove),
	.driver = {
		.name = MSM_VPE_DRV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init msm_vpe_init_module(void)
{
	return platform_driver_register(&vpe_driver);
}

static void __exit msm_vpe_exit_module(void)
{
	platform_driver_unregister(&vpe_driver);
}

module_init(msm_vpe_init_module);
module_exit(msm_vpe_exit_module);
MODULE_DESCRIPTION("MSM VPE driver");
MODULE_LICENSE("GPL v2");
