/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __APUSYS_MDW_MEM_H__
#define __APUSYS_MDW_MEM_H__

int mdw_mem_init(struct mdw_device *mdev);
void mdw_mem_deinit(struct mdw_device *mdev);
int mdw_mem_free(struct mdw_fpriv *mpriv, struct mdw_mem *mem);
int mdw_mem_dma_alloc(struct mdw_fpriv *mpriv, struct mdw_mem *mem);
int mdw_mem_dma_free(struct mdw_fpriv *mpriv, struct mdw_mem *mem);
int mdw_mem_dma_map(struct mdw_fpriv *mpriv, struct mdw_mem *mem);
int mdw_mem_dma_unmap(struct mdw_fpriv *mpriv, struct mdw_mem *mem);
int mdw_mem_dma_import(struct mdw_fpriv *mpriv, struct mdw_mem *mem);
int mdw_mem_dma_unimport(struct mdw_fpriv *mpriv, struct mdw_mem *mem);

#endif
