/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <media/videobuf2-dma-contig.h>
#include "mtk_rsc-dev.h"

static struct platform_device *mtk_rsc_dev_of_find_smem_dev
	(struct platform_device *pdev);

/* Initliaze a mtk_rsc_dev representing a completed HW RSC */
/* device */
int mtk_rsc_dev_init(struct mtk_rsc_dev *rsc_dev,
		    struct platform_device *pdev,
		    struct media_device *media_dev,
		    struct v4l2_device *v4l2_dev)
{
	int r = 0;

	rsc_dev->pdev = pdev;

	mutex_init(&rsc_dev->lock);
	atomic_set(&rsc_dev->qbuf_barrier, 0);
	init_waitqueue_head(&rsc_dev->buf_drain_wq);

	/* v4l2 sub-device registration */
	r = mtk_rsc_dev_mem2mem2_init(rsc_dev, media_dev, v4l2_dev);

	if (r) {
		dev_dbg(&rsc_dev->pdev->dev,
			"failed to create V4L2 devices (%d)\n", r);
		goto failed_mem2mem2;
	}

	return 0;

failed_mem2mem2:
	mutex_destroy(&rsc_dev->lock);
	return r;
}

int mtk_rsc_dev_get_total_node(struct mtk_rsc_dev *mtk_rsc_dev)
{
	return mtk_rsc_dev->ctx.queues_attr.total_num;
}

int mtk_rsc_dev_mem2mem2_init(struct mtk_rsc_dev *rsc_dev,
			     struct media_device *media_dev,
			     struct v4l2_device *v4l2_dev)
{
	int r, i;
	const int queue_master = rsc_dev->ctx.queues_attr.master;

	pr_debug("mem2mem2.name: %s\n", rsc_dev->ctx.device_name);
	rsc_dev->mem2mem2.name = rsc_dev->ctx.device_name;
	rsc_dev->mem2mem2.model = rsc_dev->ctx.device_name;
	rsc_dev->mem2mem2.num_nodes =
		mtk_rsc_dev_get_total_node(rsc_dev);
	rsc_dev->mem2mem2.vb2_mem_ops = &vb2_dma_contig_memops;
	rsc_dev->mem2mem2.buf_struct_size =
		sizeof(struct mtk_rsc_dev_buffer);

	rsc_dev->mem2mem2.nodes = rsc_dev->mem2mem2_nodes;
	rsc_dev->mem2mem2.dev = &rsc_dev->pdev->dev;

	for (i = 0; i < rsc_dev->ctx.dev_node_num; i++) {
		rsc_dev->mem2mem2.nodes[i].name =
			mtk_rsc_dev_get_node_name(rsc_dev, i);
		rsc_dev->mem2mem2.nodes[i].output =
				(!rsc_dev->ctx.queue[i].desc.capture);
		rsc_dev->mem2mem2.nodes[i].immutable = false;
		rsc_dev->mem2mem2.nodes[i].enabled = false;
		atomic_set(&rsc_dev->mem2mem2.nodes[i].sequence, 0);
	}

	/* Master queue is always enabled */
	rsc_dev->mem2mem2.nodes[queue_master].immutable = true;
	rsc_dev->mem2mem2.nodes[queue_master].enabled = true;

	pr_debug("register v4l2 for %llx\n",
		(unsigned long long)rsc_dev);
	r = mtk_rsc_mem2mem2_v4l2_register(rsc_dev, media_dev, v4l2_dev);

	if (r) {
		pr_debug("v4l2 init failed, dev(ctx:%d)\n",
			rsc_dev->ctx.ctx_id);
		return r;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_rsc_dev_mem2mem2_init);

void mtk_rsc_dev_mem2mem2_exit(struct mtk_rsc_dev *rsc_dev)
{
	mtk_rsc_v4l2_unregister(rsc_dev);
}
EXPORT_SYMBOL_GPL(mtk_rsc_dev_mem2mem2_exit);

char *mtk_rsc_dev_get_node_name
	(struct mtk_rsc_dev *rsc_dev, int node)
{
	struct mtk_rsc_ctx_queue_desc *mapped_queue_desc =
		&rsc_dev->ctx.queue[node].desc;

	return mapped_queue_desc->name;
}

/* Get a free buffer from a video node */
static struct mtk_rsc_ctx_buffer __maybe_unused *mtk_rsc_dev_queue_getbuf
	(struct mtk_rsc_dev *rsc_dev, int node)
{
	struct mtk_rsc_dev_buffer *buf;
	int queue = -1;

	if (node > rsc_dev->ctx.dev_node_num || node < 0) {
		dev_dbg(&rsc_dev->pdev->dev, "Invalid mtk_rsc_dev node.\n");
		return NULL;
	}

	/* Get the corrosponding queue id of the video node */
	/* Currently the queue id is the same as the node number */
	queue = node;

	if (queue < 0) {
		dev_dbg(&rsc_dev->pdev->dev, "Invalid mtk_rsc_dev node.\n");
		return NULL;
	}

	/* Find first free buffer from the node */
	list_for_each_entry(buf, &rsc_dev->mem2mem2.nodes[node].buffers,
			    m2m2_buf.list) {
		if (mtk_rsc_ctx_get_buffer_state(&buf->ctx_buf)
			== MTK_RSC_CTX_BUFFER_NEW)
			return &buf->ctx_buf;
	}

	/* There were no free buffers*/
	return NULL;
}

int mtk_rsc_dev_get_queue_id_of_dev_node(struct mtk_rsc_dev *rsc_dev,
					struct mtk_rsc_dev_video_device *node)
{
	return (node - rsc_dev->mem2mem2.nodes);
}
EXPORT_SYMBOL_GPL(mtk_rsc_dev_get_queue_id_of_dev_node);

int mtk_rsc_dev_queue_buffers(struct mtk_rsc_dev *rsc_dev,
			     bool initial)
{
	unsigned int node;
	int r = 0;
	struct mtk_rsc_dev_buffer *ibuf;
	struct mtk_rsc_ctx_frame_bundle bundle;
	const int mtk_rsc_dev_node_num = mtk_rsc_dev_get_total_node(rsc_dev);
	const int queue_master = rsc_dev->ctx.queues_attr.master;

	memset(&bundle, 0, sizeof(struct mtk_rsc_ctx_frame_bundle));

	pr_debug("%s, init(%d)\n", __func__, initial);

	if (!mtk_rsc_ctx_is_streaming(&rsc_dev->ctx)) {
		pr_debug("%s: stream off, no hw enqueue triggered\n", __func__);
		return 0;
	}

	mutex_lock(&rsc_dev->lock);

	/* Buffer set is queued to background driver (e.g. DIP, FD, and P1) */
	/* only when master input buffer is ready */
	if (!mtk_rsc_dev_queue_getbuf(rsc_dev, queue_master)) {
		mutex_unlock(&rsc_dev->lock);
		return 0;
	}

	/* Check all node from the node after the master node */
	for (node = (queue_master + 1) % mtk_rsc_dev_node_num;
		1; node = (node + 1) % mtk_rsc_dev_node_num) {
		pr_debug("Check node(%d), queue enabled(%d), node enabled(%d)\n",
			node, rsc_dev->queue_enabled[node],
			rsc_dev->mem2mem2.nodes[node].enabled);

		/* May skip some node according the scenario in the future */
		if (rsc_dev->queue_enabled[node] ||
		    rsc_dev->mem2mem2.nodes[node].enabled) {
			struct mtk_rsc_ctx_buffer *buf =
				mtk_rsc_dev_queue_getbuf(rsc_dev, node);
			char *node_name =
				mtk_rsc_dev_get_node_name(rsc_dev, node);

			if (!buf) {
				dev_dbg(&rsc_dev->pdev->dev,
					"No free buffer of enabled node %s\n",
					node_name);
				break;
			}

			/* To show the debug message */
			ibuf = container_of(buf,
					    struct mtk_rsc_dev_buffer, ctx_buf);
			dev_dbg(&rsc_dev->pdev->dev,
				"may queue user %s buffer idx(%d) to ctx\n",
				node_name,
				ibuf->m2m2_buf.vbb.vb2_buf.index);
			mtk_rsc_ctx_frame_bundle_add(&rsc_dev->ctx,
						    &bundle, buf);
		}

		/* Stop if there is no free buffer in master input node */
		if (node == queue_master) {
			if (mtk_rsc_dev_queue_getbuf(rsc_dev, queue_master)) {
				/* Has collected all buffer required */
				mtk_rsc_ctx_trigger_job(&rsc_dev->ctx, &bundle);
			} else {
				pr_debug("no new buffer found in master node, not trigger job\n");
				break;
			}
		}
	}
	mutex_unlock(&rsc_dev->lock);

	if (r && r != -EBUSY)
		goto failed;

	return 0;

failed:
	/*
	 * On error, mark all buffers as failed which are not
	 * yet queued to CSS
	 */
	dev_dbg(&rsc_dev->pdev->dev,
		"failed to queue buffer to ctx on queue %i (%d)\n",
		node, r);

	if (initial)
		/* If we were called from streamon(), no need to finish bufs */
		return r;

	for (node = 0; node < mtk_rsc_dev_node_num; node++) {
		struct mtk_rsc_dev_buffer *buf, *buf0;

		if (!rsc_dev->queue_enabled[node])
			continue;	/* Skip disabled queues */

		mutex_lock(&rsc_dev->lock);
		list_for_each_entry_safe
			(buf, buf0,
			 &rsc_dev->mem2mem2.nodes[node].buffers,
			 m2m2_buf.list) {
			if (mtk_rsc_ctx_get_buffer_state(&buf->ctx_buf) ==
				MTK_RSC_CTX_BUFFER_PROCESSING)
				continue;	/* Was already queued, skip */

			mtk_rsc_v4l2_buffer_done(&buf->m2m2_buf.vbb.vb2_buf,
						VB2_BUF_STATE_ERROR);
		}
		mutex_unlock(&rsc_dev->lock);
	}

	return r;
}
EXPORT_SYMBOL_GPL(mtk_rsc_dev_queue_buffers);

int mtk_rsc_dev_core_init(struct platform_device *pdev,
			 struct mtk_rsc_dev *rsc_dev,
			 struct mtk_rsc_ctx_desc *ctx_desc)
{
	return mtk_rsc_dev_core_init_ext(pdev,
		rsc_dev, ctx_desc, NULL, NULL);
}
EXPORT_SYMBOL_GPL(mtk_rsc_dev_core_init);

int mtk_rsc_dev_core_init_ext(struct platform_device *pdev,
			     struct mtk_rsc_dev *rsc_dev,
			     struct mtk_rsc_ctx_desc *ctx_desc,
			     struct media_device *media_dev,
			     struct v4l2_device *v4l2_dev)
{
	int r;
	struct platform_device *smem_dev = NULL;

	smem_dev = mtk_rsc_dev_of_find_smem_dev(pdev);

	if (!smem_dev)
		dev_dbg(&pdev->dev, "failed to find smem_dev\n");

	/* Device context must be initialized before device instance */
	r = mtk_rsc_ctx_core_init(&rsc_dev->ctx, pdev,
				 0, ctx_desc, pdev, smem_dev);

	dev_dbg(&pdev->dev, "init rsc_dev: %llx\n",
		 (unsigned long long)rsc_dev);
	/* init other device level members */
	mtk_rsc_dev_init(rsc_dev, pdev, media_dev, v4l2_dev);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_rsc_dev_core_init_ext);

int mtk_rsc_dev_core_release(struct platform_device *pdev,
			    struct mtk_rsc_dev *rsc_dev)
{
	mtk_rsc_dev_mem2mem2_exit(rsc_dev);
	mtk_rsc_ctx_core_exit(&rsc_dev->ctx);
	mutex_destroy(&rsc_dev->lock);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_rsc_dev_core_release);

static struct platform_device *mtk_rsc_dev_of_find_smem_dev
	(struct platform_device *pdev)
{
	struct device_node *smem_dev_node = NULL;

	if (!pdev) {
		pr_debug("Find_smem_dev failed, pdev can't be NULL\n");
		return NULL;
	}

	smem_dev_node = of_parse_phandle(pdev->dev.of_node,
					 "smem_device", 0);

	if (!smem_dev_node) {
		dev_dbg(&pdev->dev,
			"failed to find isp smem device for (%s)\n",
			pdev->name);
		return NULL;
	}

	dev_dbg(&pdev->dev, "smem of node found, try to discovery device\n");
	return of_find_device_by_node(smem_dev_node);
}

