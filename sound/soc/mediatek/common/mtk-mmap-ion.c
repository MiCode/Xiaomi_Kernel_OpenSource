// SPDX-License-Identifier: GPL-2.0
//
// mtk-mmap-ion.c  --  Mediatek Audio MMAP Get ION Buffer
//
// Copyright (c) 2017 MediaTek Inc.
// Author: Kai Chieh Chuang <kaichieh.chuang@mediatek.com>

#include <linux/io.h>
#include <linux/module.h>

#include "mtk-mmap-ion.h"

#include "ion_drv.h"
#include "ion_priv.h"
#include "ion.h"


#define ION_HEAP_CARVEOUT_MASK (1 << ION_HEAP_TYPE_CARVEOUT)


static struct ion_client *ion_client;

static struct ion_handle *dl_ion_handle;
static struct ion_handle *ul_ion_handle;

static unsigned long dl_phy_addr;
static unsigned long ul_phy_addr;

static void *dl_vir_addr;
static void *ul_vir_addr;

static size_t dl_size;
static size_t ul_size;

static int dl_fd;
static int ul_fd;


int mtk_free_ion_buffer(struct ion_handle *handle, int *fd,
			unsigned long *phy_addr,  void **vir_addr,
			size_t *size)
{
	pr_debug("+%s()\n", __func__);

	if (*vir_addr != NULL && handle != NULL)
		ion_unmap_kernel(ion_client, handle);

	if (handle != NULL)
		ion_free(ion_client, handle);

	handle = NULL;
	*vir_addr = NULL;

	*fd = 0;
	*phy_addr = 0;
	*size = 0;

	pr_debug("-%s()\n", __func__);
	return 0;
}


int mtk_alloc_ion_buffer(struct ion_handle *handle, int *fd,
			unsigned long *phy_addr, void **vir_addr,
			size_t *size)
{
	int ret = 0;

	pr_debug("+%s()\n", __func__);

	// alloc handler
	pr_debug("%s(), ion_alloc\n", __func__);
	handle = ion_alloc(ion_client, MMAP_BUFFER_SIZE, 0,
				ION_HEAP_CARVEOUT_MASK, 0);
	if (IS_ERR(handle)) {
		pr_debug("%s, DL ion_alloc fail %p\n", __func__, handle);
		handle = NULL;
		goto exit;
	}

	// get physical addr & size
	pr_debug("%s(), ion_phys\n", __func__);
	ret = ion_phys(ion_client, handle, phy_addr, size);
	if (ret != 0) {
		pr_debug("%s, ion_phys fail, ret %d\n", __func__, ret);
		goto exit;
	}

	// get fd
	pr_debug("%s(), ion_share_dma_buf_fd\n", __func__);
	*fd = ion_share_dma_buf_fd(ion_client, handle);
	if (*fd <= 0) {
		pr_debug("ion_share_dma_buf_fd fail, fd %d\n", *fd);
		goto exit;
	}

	// get virtual addr
	pr_debug("%s(), ion_map_kernel\n", __func__);
	*vir_addr = ion_map_kernel(ion_client, handle);
	if (*vir_addr == NULL) {
		pr_debug("%s, ion_map_kernel fail\n", __func__);
		goto exit;
	}


	pr_debug("-%s(), handle %p, addr %p, phy_addr %ld, size %zu, fd %d\n",
			__func__, handle, *vir_addr, *phy_addr, *size, *fd);
	return 0;

exit:
	mtk_free_ion_buffer(handle, fd, phy_addr, vir_addr, size);
	pr_debug("-%s(), fail\n", __func__);
	return -1;
}


int mtk_get_ion_buffer(void)
{
	int ret;

	pr_debug("+%s()\n", __func__);

	if (ion_client != NULL) {
		pr_debug("mtk_get_ion free+\n");
		mtk_free_ion_buffer(dl_ion_handle, &dl_fd, &dl_phy_addr,
					&dl_vir_addr, &dl_size);
		mtk_free_ion_buffer(ul_ion_handle, &ul_fd, &ul_phy_addr,
					&ul_vir_addr, &ul_size);
		ion_client_destroy(ion_client);
		ion_client = NULL;
		pr_debug("mtk_get_ion free-\n");
	}

	// create client
	ion_client = ion_client_create(g_ion_device, "AAudioMMAP");
	if (PTR_RET(ion_client) != 0) {
		pr_debug("%s, ion_client_create fail, device %p, client %p\n",
			__func__, g_ion_device, ion_client);
		goto exit;
	}

	pr_debug("+%s(), mtk_alloc_ion_buffer for DL\n", __func__);
	ret = mtk_alloc_ion_buffer(dl_ion_handle, &dl_fd, &dl_phy_addr,
					&dl_vir_addr, &dl_size);
	if (ret !=  0)
		goto exit;

	pr_debug("+%s(), mtk_alloc_ion_buffer for UL\n", __func__);
	ret = mtk_alloc_ion_buffer(ul_ion_handle, &ul_fd, &ul_phy_addr,
					&ul_vir_addr, &ul_size);
	if (ret !=  0) {
		mtk_free_ion_buffer(dl_ion_handle, &dl_fd, &dl_phy_addr,
					&dl_vir_addr, &dl_size);
		goto exit;
	}

	pr_debug("-%s()\n", __func__);
	return 0;

exit:
	if (ion_client != NULL) {
		ion_client_destroy(ion_client);
		ion_client = NULL;
	}
	pr_debug("-%s(), fail\n", __func__);
	return -1;
}


int mtk_get_mmap_dl_fd(void)
{
	return dl_fd;
}


int mtk_get_mmap_ul_fd(void)
{
	return ul_fd;
}


void mtk_get_mmap_dl_buffer(unsigned long *phy_addr, void **vir_addr)
{
	*phy_addr = dl_phy_addr;
	*vir_addr = dl_vir_addr;
}


void mtk_get_mmap_ul_buffer(unsigned long *phy_addr, void **vir_addr)
{
	*phy_addr = ul_phy_addr;
	*vir_addr = ul_vir_addr;
}


MODULE_DESCRIPTION("Mediatek Smart Phone PCM Operation");
MODULE_AUTHOR("Kai Chieh Chuang <kaichieh.chuang@mediatek.com>");
MODULE_LICENSE("GPL v2");

