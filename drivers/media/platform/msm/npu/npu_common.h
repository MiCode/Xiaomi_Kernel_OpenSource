/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _NPU_COMMON_H
#define _NPU_COMMON_H
#include <linux/list.h>
#include <linux/file.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/regulator/consumer.h>
#include <linux/dma-mapping.h>
#include <linux/msm-bus.h>
#include <linux/dma-buf.h>
#include <linux/msm_dma_iommu_mapping.h>
#include <asm/dma-iommu.h>
#include <stdarg.h>
#include <linux/msm_npu.h>

/* get npu info */
#define MSM_NPU_GET_INFO_32 \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 1, compat_caddr_t)

/* map buf */
#define MSM_NPU_MAP_BUF_32 \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 2, compat_caddr_t)

/* map buf */
#define MSM_NPU_UNMAP_BUF_32 \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 3, compat_caddr_t)

/* load network */
#define MSM_NPU_LOAD_NETWORK_32 \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 4, compat_caddr_t)

/* unload network */
#define MSM_NPU_UNLOAD_NETWORK_32 \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 5, compat_caddr_t)

/* exec network */
#define MSM_NPU_EXEC_NETWORK_32 \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 6, compat_caddr_t)

#define NPU_MAX_CLK_NUM		8
#define NPU_MAX_REGULATOR_NUM	4
#define NPU_MAX_DT_NAME_LEN	16

#define NPU_FIRMWARE_VERSION	0x1000

struct npu_clk_t {
	struct clk *clk;
	char clk_name[NPU_MAX_DT_NAME_LEN];
};

struct npu_regulator_t {
	struct regulator *regulator;
	char regulator_name[NPU_MAX_DT_NAME_LEN];
};

#define DEFAULT_REG_DUMP_NUM	0x100
#define ROW_BYTES 16
#define GROUP_BYTES 4

struct npu_device_t {
	struct mutex ctx_lock;

	struct platform_device *pdev;

	dev_t dev_num;
	struct cdev cdev;
	struct class *class;
	struct device *device;

	size_t reg_size;
	char __iomem *npu_base;
	u32 npu_phys;

	uint32_t core_clk_num;
	struct npu_clk_t core_clks[NPU_MAX_CLK_NUM];

	uint32_t regulator_num;
	struct npu_regulator_t regulators[NPU_MAX_DT_NAME_LEN];

	u32 irq;
};
#endif /* _NPU_COMMON_H */
