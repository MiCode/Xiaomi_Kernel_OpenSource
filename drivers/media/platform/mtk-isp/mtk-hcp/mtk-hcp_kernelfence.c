// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <asm/cacheflush.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/file.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/freezer.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/miscdevice.h>
#include <linux/types.h>
#include <linux/dma-fence.h>
#include <linux/sync_file.h>
#include <linux/slab.h>

#include "mtk-hcp_kernelfence.h"

struct fence_info **fence_queue;
int create_task_flag;
int fence_use;
int fence_release;
#define FENCE_PAIR  10
#define FENCE_NUM   3

static DEFINE_MUTEX(fence_mutex);
static DEFINE_SPINLOCK(fence_lock);

static const char *dma_fence_get_name(struct dma_fence *fence)
{
	return "imgsys-dma-fence";
}

static const struct dma_fence_ops fence_ops = {
	.get_driver_name = dma_fence_get_name,
	.get_timeline_name = dma_fence_get_name,
};

static int create_fence(int fd_num, unsigned int *fds, struct fence_info *f_queue[])
{
	int i, index;
	struct sync_file *sync_file;

	pr_info("%s/fence_use(%d) +\n", __func__, fence_use);
	for (i = 0; i < fd_num; i++) {
		index = fence_use;
		if (f_queue[index]->use_flag != 1) {
			dma_fence_init(f_queue[index]->kFence, &fence_ops, &fence_lock, 0, 0);
			sync_file = sync_file_create(f_queue[index]->kFence);
			if (sync_file == NULL) {
				pr_info("sync_file create fail\n");
				return 0;
			}
			fds[i] = get_unused_fd_flags(O_CLOEXEC);
			pr_info("fence_fd(%d)\n", fds[i]);
			if (fds[i] > 0) {
				fd_install(fds[i], sync_file->file);
				dma_fence_get(f_queue[index]->kFence);
				f_queue[index]->use_flag = 1;
			}
		} else {
			pr_info("fence queue already full\n");
			return 1;
		}
		fence_use++;
		if (fence_use >= (FENCE_PAIR*FENCE_NUM))
			fence_use = 0;
	}

	for (i = 0; i < fd_num; i++)
		pr_info("%s kernel_fence(%d)\n", __func__, fds[i]);

	pr_debug("%s/fence_use(%d) -\n", __func__, fence_use);
	return 0;
}

static int mtk_hcp_get_KernelFence(struct device *dev,
		 enum hcp_id id, unsigned int *fds, int fd_num, struct fence_info *f_queue[])
{
	int ret;

	mutex_lock(&fence_mutex);
	/*create new fence*/
	ret = create_fence(fd_num, fds, f_queue);
	if (ret) {
		pr_info("create fence fail\n");
		mutex_unlock(&fence_mutex);
		return -EINVAL;
	}
	mutex_unlock(&fence_mutex);
	return 0;
}

static int mtk_hcp_release_KernelFence(struct device *dev,
		 enum hcp_id id, unsigned int *fds, int fd_num, struct fence_info *f_queue[])
{
	struct dma_fence *release_fence = NULL;
	int i, index;

	pr_info("imgsys release kernel fence(%d) +\n", fence_release);
	mutex_lock(&fence_mutex);
	for (i = 0; i < fd_num; i++) {
		index = fence_release;
		release_fence = sync_file_get_fence(fds[i]);
		/* dma_fence_signal(release_fence); */
		dma_fence_put(release_fence);
		f_queue[index]->use_flag = 0;
		put_unused_fd(fds[i]);
		fence_release++;
		if (fence_release >= (FENCE_PAIR*FENCE_NUM))
			fence_release = 0;
	}
	mutex_unlock(&fence_mutex);
	pr_info("imgsys release kernel fence(%d) -\n", fence_release);

	return 0;
}

int mtk_hcp_set_KernelFence(struct device *dev,
		 enum hcp_id id, unsigned int *fds, uint8_t fd_num, int get)
{
	if (get == 1)
		return mtk_hcp_get_KernelFence(dev, id, fds, fd_num, fence_queue);
	else
		return mtk_hcp_release_KernelFence(dev, id, fds, fd_num, fence_queue);
}
EXPORT_SYMBOL(mtk_hcp_set_KernelFence);

int mtk_hcp_init_KernelFence(struct device *dev)
{
	int i;

	fence_queue = kzalloc(FENCE_PAIR*FENCE_NUM*sizeof(struct fence_info *), GFP_KERNEL);
	for (i = 0; i < (FENCE_PAIR*FENCE_NUM); i++) {
		fence_queue[i] = kzalloc(sizeof(struct fence_info), GFP_KERNEL);
		fence_queue[i]->kFence = kzalloc(sizeof(struct dma_fence), GFP_KERNEL);
		fence_queue[i]->use_flag = 0;
	}
	return 0;
}
EXPORT_SYMBOL(mtk_hcp_init_KernelFence);

int mtk_hcp_uninit_KernelFence(struct device *dev)
{
	int i;

	pr_info("%s +", __func__);
	for (i = 0; i < (FENCE_PAIR*FENCE_NUM); i++) {
		kfree(fence_queue[i]->kFence);
		kfree(fence_queue[i]);
	}
	kfree(fence_queue);
	pr_info("%s -", __func__);
	return 0;
}
EXPORT_SYMBOL(mtk_hcp_uninit_KernelFence);
