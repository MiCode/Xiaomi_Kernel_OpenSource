/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include "xhci-mtk.h"

void *mtk_usb_alloc_sram(unsigned int id, size_t size, dma_addr_t *dma)
{
	return NULL;
}
EXPORT_SYMBOL_GPL(mtk_usb_alloc_sram);

void mtk_usb_free_sram(unsigned int id)
{
	return;
}
EXPORT_SYMBOL_GPL(mtk_usb_free_sram);
