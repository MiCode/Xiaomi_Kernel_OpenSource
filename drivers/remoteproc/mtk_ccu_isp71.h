/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __RPOC_MTK_CCU_IPS7_H
#define __RPOC_MTK_CCU_IPS7_H

#include <linux/kernel.h>
#include <linux/remoteproc.h>
#include <linux/wait.h>
#include <linux/types.h>

#include "mtk_ccu_common.h"

#define MTK_CCU_CORE_PMEM_BASE  (0x00000000)
#define MTK_CCU_CORE_DMEM_BASE  (0x00020000)
#define MTK_CCU_PMEM_BASE  (0x1B000000)
#define MTK_CCU_DMEM_BASE  (0x1B020000)
#define MTK_CCU_PMEM_SIZE  (0x20000)
#define MTK_CCU_DMEM_SIZE  (0x20000)
#define MTK_CCU_ISR_LOG_SIZE  (0x400)
#define MTK_CCU_LOG_SIZE  (0x800)
#define MTK_CCU_CACHE_SIZE  (0)
#define MTK_CCU_CACHE_BASE (0x40000000)
#define MTK_CCU_SHARED_BUF_OFFSET 0 //at DCCM start

#define MTK_CCU_REG_RESET    (0x0)
#define MTK_CCU_HW_RESET_BIT (0x000d0100)
#define MTK_CCU_REG_CTRL     (0x0c)
#define MTK_CCU_REG_AXI_REMAP  (0x24)
#define MTK_CCU_REG_CORE_CTRL  (0x28)
#define MTK_CCU_RUN_BIT      (0x00000010)
#define MTK_CCU_REG_CORE_STATUS     (0x28)
#define MTK_CCU_INT_TRG         (0x8010)	// (0x2C)
#define MTK_CCU_INT_CLR         (0x5C)
#define MTK_CCU_INT_ST          (0x60)
#define MTK_CCU_MON_ST          (0x78)

#define MTK_CCU_SPARE_REG00   (0x8020)
#define MTK_CCU_SPARE_REG01   (0x8024)
#define MTK_CCU_SPARE_REG02   (0x8028)
#define MTK_CCU_SPARE_REG03   (0x802C)
#define MTK_CCU_SPARE_REG04   (0x8030)
#define MTK_CCU_SPARE_REG05   (0x8034)
#define MTK_CCU_SPARE_REG06   (0x8038)
#define MTK_CCU_SPARE_REG07   (0x803C)
#define MTK_CCU_SPARE_REG08   (0x8040)
#define MTK_CCU_SPARE_REG09   (0x8044)
#define MTK_CCU_SPARE_REG10   (0x8048)
#define MTK_CCU_SPARE_REG11   (0x804C)
#define MTK_CCU_SPARE_REG12   (0x8050)
#define MTK_CCU_SPARE_REG13   (0x8054)
#define MTK_CCU_SPARE_REG14   (0x8058)
#define MTK_CCU_SPARE_REG15   (0x805C)
#define MTK_CCU_SPARE_REG16   (0x8060)
#define MTK_CCU_SPARE_REG17   (0x8064)
#define MTK_CCU_SPARE_REG18   (0x8068)
#define MTK_CCU_SPARE_REG19   (0x806C)
#define MTK_CCU_SPARE_REG20   (0x8070)
#define MTK_CCU_SPARE_REG21   (0x8074)
#define MTK_CCU_SPARE_REG22   (0x8078)
#define MTK_CCU_SPARE_REG23   (0x807C)
#define MTK_CCU_SPARE_REG24   (0x8080)
#define MTK_CCU_SPARE_REG25   (0x8084)
#define MTK_CCU_SPARE_REG26   (0x8088)
#define MTK_CCU_SPARE_REG27   (0x808C)
#define MTK_CCU_SPARE_REG28   (0x8090)
#define MTK_CCU_SPARE_REG29   (0x8094)
#define MTK_CCU_SPARE_REG30   (0x8098)
#define MTK_CCU_SPARE_REG31   (0x809C)

#define CCU_STATUS_INIT_DONE              0xffff0000
#define CCU_STATUS_INIT_DONE_2            0xffff00a5
#define CCU_GO_TO_LOAD                    0x10AD10AD
#define CCU_GO_TO_RUN                     0x17172ACE
#define CCU_GO_TO_STOP                    0x8181DEAD

#endif //__RPOC_MTK_CCU_IPS7_H
