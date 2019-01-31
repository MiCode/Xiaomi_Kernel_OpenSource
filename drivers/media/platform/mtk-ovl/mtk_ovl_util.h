/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Qing Li <qing.li@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MTK_OVL_UTIL_H
#define MTK_OVL_UTIL_H

#include <linux/bug.h>
#include <linux/clk.h>
#include <linux/errno.h>

#include <linux/io.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/mfd/syscon.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/file.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#ifdef CONFIG_MTK_IOMMU
#include <linux/iommu.h>
#endif
#include <soc/mediatek/smi.h>
//#include <asm/dma-iommu.h>
#include <asm/cacheflush.h>

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-mediabus.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-vmalloc.h>

#define SUPPORT_IOMMU_ATTACH            (0)
#define SUPPORT_PROBE_CLOCK_ON          (0)
#define SUPPORT_ALLOC_CTX               (0)
#define SUPPORT_CLOCK_SUSPEND           (0)
#define MTK_OVL_SUPPORT_TIME_GAP        (0)
#define MTK_OVL_SUPPORT_DCM             (0) /*lq-check*/
#define MTK_OVL_SUPPORT_FMT_ALL         (0) /*lq-check*/

#define DISP_MUTEX_IDX                  (9)

//#define MTK_OVL_MAIN_NAME               "1402c000.ovl"
#define MTK_OVL_MAIN_ID                 (0)

#define INVALID_DW                      (0xffffffff)
#define INVALID_DECIMAL                 (99)

#define RET_OK                          (0)
#define RET_ERR_PARAM                   (1)
#define RET_ERR_EXCEPTION               (2)

#define OVL_MAX_WIDTH                   (8191)
#define OVL_MAX_HEIGHT                  (4095)
#define OVL_LAYER_NUM                   (4)

#define REG_FLD_WIDTH(field) \
	((unsigned int)(((field) >> 16) & 0xFF))
#define REG_FLD_SHIFT(field) \
	((unsigned int)((field) & 0xFF))
#define REG_FLD_MASK(field) \
	(((unsigned int) \
	(1 << REG_FLD_WIDTH((field))) - 1) << REG_FLD_SHIFT((field)))
#define REG_FLD_VAL(field, val) \
	(((val) << REG_FLD_SHIFT((field))) & REG_FLD_MASK((field)))
#define REG_FLD(width, shift) \
	((unsigned int)((((width) & 0xFF) << 16) | ((shift) & 0xFF)))
#define DISP_REG_SET(addr, val) \
	(*((unsigned int *)(addr)) = (val))
#define DISP_REG_SET_FIELD(field, addr, val) \
	(*((unsigned int *)(addr)) |= \
		((val) << REG_FLD_SHIFT((field))) & REG_FLD_MASK((field)))
#define DISP_REG_GET(addr) \
	(*(unsigned int *)((unsigned long)(addr)))

/******************************************************************************/

#define LOG_TAG "[MTK_OVL]"
#define log_dbg(fmt, ...) \
	do {if (0) pr_info("%s "fmt"\n", LOG_TAG, ##__VA_ARGS__); } \
	while (0)
#define log_err(fmt, ...) \
	do {if (1) pr_info("%s [E] "fmt"\n", LOG_TAG, ##__VA_ARGS__); } \
	while (0)

#define LIST_OLD 0

#define FMT_IS_420(fmt) 1

#endif /* MTK_OVL_UTIL_H */

