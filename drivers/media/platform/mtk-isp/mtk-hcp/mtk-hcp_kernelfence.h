/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_HCP_KERNELFENCE_H
#define MTK_HCP_KERNELFENCE_H

#include <linux/fdtable.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>

#include "mtk-hcp.h"

struct fence_info {
	struct list_head fence_entry;
	struct dma_fence kFence;
	unsigned int use_fence;
	unsigned int release_fence;
	unsigned int fd;
	pid_t tgid;
	char name[32];
	spinlock_t lock;
};

struct fence_list {
	struct list_head list;
	struct mutex fence_lock;
};

/**
 * mtk_hcp_set_KernelFence - send data from camera kernel driver to HCP without
 *      waiting demand receives the command.
 *
 * @pdev:   HCP platform device
 * @id:     HCP ID
 * @buf:    the data buffer
 * @len:    the data buffer length
 * @get:    get fence or release fence (1:get | 0:release)
 *
 * This function is thread-safe. When this function returns,
 * HCP has received the data and save the data in the workqueue.
 * After that it will schedule work to dequeue to send data to CM4 or
 * RED for programming register.
 * Return: Return 0 if sending data successfully, otherwise it is failed.
 **/
int mtk_hcp_set_KernelFence(unsigned int *fds, uint8_t fd_num, int get);

/**
 * mtk_hcp_set_KernelFence - send data from camera kernel driver to HCP without
 *      waiting demand receives the command.
 *
 * @pdev:   HCP platform device
 * @id:     HCP ID
 * @buf:    the data buffer
 * @len:    the data buffer length
 * @get:    get fence or release fence (1:get | 0:release)
 *
 * This function is thread-safe. When this function returns,
 * HCP has received the data and save the data in the workqueue.
 * After that it will schedule work to dequeue to send data to CM4 or
 * RED for programming register.
 * Return: Return 0 if sending data successfully, otherwise it is failed.
 **/
int mtk_hcp_uninit_KernelFence(void);

int mtk_hcp_init_KernelFence(void);

#endif /* _MTK_HCP_KERNELFENCE_H */
