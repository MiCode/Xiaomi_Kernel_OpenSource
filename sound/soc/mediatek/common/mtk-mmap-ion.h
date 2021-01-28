/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-mmap-ion.h  --  Mediatek Smart Phone PCM Operation
 *
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Kai Chieh Chuang <kaichieh.chuang@mediatek.com>
 */

#ifndef _MTK_MMAP_ION_H_
#define _MTK_MMAP_ION_H_

#define MMAP_BUFFER_SIZE    8192

int mtk_get_ion_buffer(void);

int mtk_get_mmap_dl_fd(void);
int mtk_get_mmap_ul_fd(void);

void mtk_get_mmap_dl_buffer(unsigned long *phy_addr, void **vir_addr);
void mtk_get_mmap_ul_buffer(unsigned long *phy_addr, void **vir_addr);

#endif

