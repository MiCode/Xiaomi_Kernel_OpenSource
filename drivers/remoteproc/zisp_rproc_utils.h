/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Xiaomi, Inc.
 */

#ifndef XIAOMI_ZISP_RPROC_UTILS_H
#define XIAOMI_ZISP_RPROC_UTILS_H

#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/remoteproc.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <uapi/linux/sched/types.h>

#include "remoteproc_internal.h"

struct zisp_rproc;

struct zisp_rproc_ipi {
	atomic_t recv_wakeup;
	struct rproc *rproc;
	struct task_struct *recv_thread;
	irqreturn_t (*rx_callback)(int irq, void *data);
	int irq;
};

struct zisp_ops {
	int (*init)(struct zisp_rproc *zisp_rproc);
	void (*shutdown)(struct zisp_rproc *zisp_rproc);
	void (*remove)(struct zisp_rproc *zisp_rproc);
	int (*parse_regmap)(struct platform_device *pdev,
			    void __iomem *regs);
	int (*parse_mem)(struct platform_device *pdev,
			 struct resource **_iores);
	int (*parse_irq)(struct platform_device *pdev,
			 int irq);
	struct rproc_ops *rproc_ops;
	const char *firmware;
};

struct zisp_rproc_mem {
	void *virt_addr;
	phys_addr_t phys_addr;
	dma_addr_t dma_addr;
	dma_addr_t rbase;
	size_t size;
};

struct zisp_rproc {
	struct rproc *rproc;
	struct device *dev;
	struct spi_device *spi;
	struct regmap *regmap;
	struct zisp_ops *zisp_ops;
	struct zisp_rproc_mem *memlist;
	struct zisp_rproc_ipi ipi;
	dma_addr_t ipc_membase;
	dma_addr_t ipc_dma_addr;
	size_t ipc_size;
	unsigned int reg_base;
	uint8_t *local_buf;
};

int zisp_rproc_ipi_setup(struct zisp_rproc_ipi *ipi, struct rproc *rproc);
void zisp_rproc_ipi_teardown(struct zisp_rproc_ipi *ipi);

extern struct zisp_ops zisp3_ops_pci;
extern struct zisp_ops zisp3_ops_spi;

#endif //XIAOMI_ZISP_RPROC_UTILS_H
