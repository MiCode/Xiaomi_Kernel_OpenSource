// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fdtable.h>
#include <linux/uaccess.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/platform_data/mtk_ccd_controls.h>
#include <linux/platform_data/mtk_ccd.h>
#include <linux/remoteproc/mtk_ccd_mem.h>
#include <linux/rpmsg/mtk_ccd_rpmsg.h>

#define CCD_ALLOCATE_MAX_BUFFER_SIZE 0x8000000UL /*128MB*/

struct mtk_ccd_memory *mtk_ccd_mem_init(struct device *dev)
{
	struct mtk_ccd_memory *ccd_memory;

	ccd_memory = kzalloc(sizeof(*ccd_memory), GFP_KERNEL);
	if (!ccd_memory)
		return NULL;

	ccd_memory->mem_ops = &vb2_dma_contig_memops;
	ccd_memory->dev = dev;
	ccd_memory->num_buffers = 0;
	mutex_init(&ccd_memory->mmap_lock);

	return ccd_memory;
}
EXPORT_SYMBOL_GPL(mtk_ccd_mem_init);

void mtk_ccd_mem_release(struct mtk_ccd *ccd)
{
	struct mtk_ccd_memory *ccd_memory = ccd->ccd_memory;

	kfree(ccd_memory);
}
EXPORT_SYMBOL_GPL(mtk_ccd_mem_release);

void *mtk_ccd_get_buffer(struct mtk_ccd *ccd,
			 struct ccd_mem_obj  *mem_buff_data)
{
	void *va, *da;
	unsigned int buffers;
	struct mtk_ccd_mem *ccd_buffer;
	struct mtk_ccd_memory *ccd_memory = ccd->ccd_memory;

	mem_buff_data->iova = 0;
	mem_buff_data->va = 0;
	mem_buff_data->pa = 0;

	mutex_lock(&ccd_memory->mmap_lock);
	buffers = ccd_memory->num_buffers;
	if (mem_buff_data->len > CCD_ALLOCATE_MAX_BUFFER_SIZE ||
	    mem_buff_data->len == 0U ||
	    buffers >= MAX_NUMBER_OF_BUFFER) {
		dev_err(ccd_memory->dev,
			"%s: Failed: buffer len = %u num_buffers = %d !!\n",
			 __func__, mem_buff_data->len, buffers);
		return ERR_PTR(-EINVAL);
	}

	ccd_buffer = &ccd_memory->bufs[buffers];
	ccd_buffer->mem_priv = ccd_memory->mem_ops->alloc(ccd_memory->dev, 0,
		mem_buff_data->len, 0, 0);
	ccd_buffer->size = mem_buff_data->len;
	if (IS_ERR(ccd_buffer->mem_priv)) {
		dev_err(ccd_memory->dev, "%s: CQ buf allocation failed\n",
			__func__);
		mutex_unlock(&ccd_memory->mmap_lock);
		goto free;
	}

	va = ccd_memory->mem_ops->vaddr(ccd_buffer->mem_priv);
	da = ccd_memory->mem_ops->cookie(ccd_buffer->mem_priv);

	mem_buff_data->iova = *(dma_addr_t *)da;
	mem_buff_data->va = (unsigned long)va;
	/* TBD: No iommu case only */
	mem_buff_data->pa = *(dma_addr_t *)da;
	ccd_memory->num_buffers++;
	mutex_unlock(&ccd_memory->mmap_lock);

	dev_info(ccd_memory->dev,
		"Num_bufs = %d iova = %x va = %llx size = %d priv = %lx\n",
		 ccd_memory->num_buffers, mem_buff_data->iova,
		 mem_buff_data->va,
		 (unsigned int)ccd_buffer->size,
		 (unsigned long)ccd_buffer->mem_priv);

	return ccd_buffer->mem_priv;
free:
	ccd_memory->mem_ops->put(ccd_buffer->mem_priv);
	return ERR_PTR(-ENOMEM);
}
EXPORT_SYMBOL_GPL(mtk_ccd_get_buffer);

int mtk_ccd_free_buffer(struct mtk_ccd *ccd,
			struct ccd_mem_obj  *mem_buff_data)
{
	void *va, *da;
	int ret = -EINVAL;
	struct mtk_ccd_mem *ccd_buffer;
	unsigned int buffer, num_buffers, last_buffer;
	struct mtk_ccd_memory *ccd_memory = ccd->ccd_memory;

	mutex_lock(&ccd_memory->mmap_lock);
	num_buffers = ccd_memory->num_buffers;
	if (num_buffers != 0U) {
		for (buffer = 0; buffer < num_buffers; buffer++) {
			ccd_buffer = &ccd_memory->bufs[buffer];
			va = ccd_memory->mem_ops->vaddr(ccd_buffer->mem_priv);
			da = ccd_memory->mem_ops->cookie(ccd_buffer->mem_priv);

			if (mem_buff_data->va == (unsigned long)va &&
			    mem_buff_data->iova == *(dma_addr_t *)da &&
			    mem_buff_data->len == ccd_buffer->size) {
				dev_info(ccd_memory->dev,
					"Free buff = %d iova = %x va = %llx, queue_num = %d\n",
					 buffer, mem_buff_data->iova,
					 mem_buff_data->va,
					 num_buffers);
				ccd_memory->mem_ops->put(ccd_buffer->mem_priv);
				last_buffer = num_buffers - 1U;
				if (last_buffer != buffer)
					ccd_memory->bufs[buffer] =
						ccd_memory->bufs[last_buffer];

				ccd_memory->bufs[last_buffer].mem_priv = NULL;
				ccd_memory->bufs[last_buffer].size = 0;
				ccd_memory->num_buffers--;
				ret = 0;
				break;
			}
		}
	}
	mutex_unlock(&ccd_memory->mmap_lock);

	if (ret != 0)
		dev_info(ccd_memory->dev,
			"Can not free memory va %llx iova %x len %u!\n",
			 mem_buff_data->va, mem_buff_data->iova,
			 mem_buff_data->len);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_ccd_free_buffer);

struct dma_buf *
mtk_ccd_get_dmabuf(struct mtk_ccd *ccd, void *mem_priv)
{
	struct dma_buf *dmabuf;
	struct mtk_ccd_memory *ccd_memory = ccd->ccd_memory;

	dmabuf = ccd_memory->mem_ops->get_dmabuf(mem_priv, O_RDWR);
	return dmabuf;
}
EXPORT_SYMBOL_GPL(mtk_ccd_get_dmabuf);


int mtk_ccd_get_dmabuf_fd(struct mtk_ccd *ccd,
			  struct dma_buf *dmabuf,
			  int ori_fd)
{
	struct task_struct *task = NULL;
	struct files_struct *f = NULL;
	struct sighand_struct *sighand;
	spinlock_t siglock;
	int target_fd = 0;

	if (dmabuf == NULL || dmabuf->file == NULL)
		return 0;

	mtk_ccd_get_serivce(ccd, &task, &f);
	if (task == NULL || f == NULL ||
		probe_kernel_address(&task->sighand, sighand) ||
		probe_kernel_address(&task->sighand->siglock, siglock))
		return -EMFILE;

	dev_dbg(ccd->dev, "Master pid:%d, tgid:%d; current pid:%d, tgid:%d",
		task->pid, task->tgid, current->pid, current->tgid);

	if (task->tgid != current->tgid) {
		dev_err(ccd->dev,
			"User process(%d) must be the same as CCD(%d)\n",
			task->tgid, current->tgid);
		return -EINVAL;
	}

	if (ori_fd > 0)
		return ori_fd;

	get_file(dmabuf->file);
	target_fd = get_unused_fd_flags(O_CLOEXEC);
	if (target_fd < 0) {
		dev_dbg(ccd->dev, "failed to get fd: %d\n");
		return -EMFILE;
	}

	fd_install(target_fd, dmabuf->file);

	return target_fd;
}
EXPORT_SYMBOL_GPL(mtk_ccd_get_dmabuf_fd);

void mtk_ccd_put_fd(struct mtk_ccd *ccd, unsigned int target_fd)
{
	struct task_struct *task = NULL;
	struct files_struct *f = NULL;

	mtk_ccd_get_serivce(ccd, &task, &f);
	if (task == NULL || f == NULL)
		return;

	__close_fd(f, target_fd);
}
EXPORT_SYMBOL_GPL(mtk_ccd_put_fd);

void mtk_ccd_get_serivce(struct mtk_ccd *ccd,
			 struct task_struct **task,
			 struct files_struct **f)
{
	dev_info(ccd->dev, "service: %p\n", ccd->ccd_masterservice);
	*task = ccd->ccd_masterservice;
	*f = ccd->ccd_files;
}
EXPORT_SYMBOL_GPL(mtk_ccd_get_serivce);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek ccd memory interface");
