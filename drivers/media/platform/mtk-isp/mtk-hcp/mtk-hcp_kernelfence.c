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

struct fence_list *fence_info_list;


static const char *dma_fence_get_name(struct dma_fence *fence)
{
	return "imgsys-dma-fence";
}

static void dma_fence_cb_release(struct dma_fence *fence)
{
	struct fence_info *f_entry;
	struct fence_info *f_entry_temp;

	pr_debug("release fence callback +\n");
	/* look up used fence entry */
	mutex_lock(&fence_info_list->fence_lock);
	list_for_each_entry_safe(f_entry, f_entry_temp, &fence_info_list->list, fence_entry) {
		/* free fence */
		if (f_entry->release_fence == 1) {
			pr_info("found imgsys_fw-%p free\n", &f_entry->kFence);
			list_del(&f_entry->fence_entry);
			kfree(f_entry);
		}
	}
	pr_debug("release fence callback -\n");
	mutex_unlock(&fence_info_list->fence_lock);
	pr_debug("release fence callback -1\n");
}

static const struct dma_fence_ops fence_ops = {
	.get_driver_name = dma_fence_get_name,
	.get_timeline_name = dma_fence_get_name,
	.release = dma_fence_cb_release,
};

static int mtk_hcp_acquire_KernelFence(unsigned int *fds, int fd_num)
{
	int i;
	struct fence_info *f_entry;
	struct sync_file *sync_file;

	/*create new fence*/
	mutex_lock(&fence_info_list->fence_lock);
	for (i = 0; i < fd_num; i++) {
		/* add user to list head */
		f_entry = kzalloc(sizeof(struct fence_info), GFP_KERNEL);
		dma_fence_init(&f_entry->kFence, &fence_ops, &f_entry->lock, 0, 0);
		sync_file = sync_file_create(&f_entry->kFence);
		if (sync_file == NULL) {
			pr_info("sync_file create fail\n");
			return -EINVAL;
		}
		fds[i] = get_unused_fd_flags(O_CLOEXEC);
		pr_debug("imgsys_fw-fence-fd(%d)\n", fds[i]);
		if (fds[i] > 0) {
			fd_install(fds[i], sync_file->file);
			f_entry->use_fence = 1;
			f_entry->fd = fds[i];
		}
		list_add_tail(&f_entry->fence_entry, &fence_info_list->list);
	}
	mutex_unlock(&fence_info_list->fence_lock);
	return 0;
}

static int mtk_hcp_release_KernelFence(unsigned int *fds, int fd_num)
{
	int i;
	struct fence_info *f_entry;
	struct fence_info *f_entry_temp;

	pr_debug("set release fence flag\n");
	mutex_lock(&fence_info_list->fence_lock);
	for (i = 0; i < fd_num; i++) {
		/* look up used fence entry */
		list_for_each_entry_safe(f_entry, f_entry_temp, &fence_info_list->list,
				fence_entry) {
		/* set release fence flag*/
			if ((f_entry->fd == fds[i]) && (f_entry->use_fence == 1)) {
				pr_info("set fence release imgsys_fw-(%d/%d/%p)\n",
						i, fds[i], &f_entry->kFence);
				f_entry->release_fence = 1;
				mutex_unlock(&fence_info_list->fence_lock);
				dma_fence_put(&f_entry->kFence);
				break;
			}
		}
		pr_debug("imgsys_fw-(%d/%d)--\n", i, fds[i]);
	}
	return 0;
}

int mtk_hcp_set_KernelFence(unsigned int *fds, uint8_t fd_num, int get)
{
	if (get == 1)
		return mtk_hcp_acquire_KernelFence(fds, fd_num);
	else
		return mtk_hcp_release_KernelFence(fds, fd_num);
}
EXPORT_SYMBOL(mtk_hcp_set_KernelFence);

int mtk_hcp_init_KernelFence(void)
{
	fence_info_list = kzalloc(sizeof(struct fence_list), GFP_KERNEL);
	INIT_LIST_HEAD(&fence_info_list->list);
	mutex_init(&fence_info_list->fence_lock);
	return 0;
}
EXPORT_SYMBOL(mtk_hcp_init_KernelFence);

int mtk_hcp_uninit_KernelFence(void)
{
	pr_info("imgsys_fw-fence uninit");
	if (list_empty(&fence_info_list->list)) {
		pr_info("imgsys_fw-list empty");
		mutex_destroy(&fence_info_list->fence_lock);
		kfree(fence_info_list);
	} else
		pr_info("imgsys_fw-list not empty");

	return 0;
}
EXPORT_SYMBOL(mtk_hcp_uninit_KernelFence);
