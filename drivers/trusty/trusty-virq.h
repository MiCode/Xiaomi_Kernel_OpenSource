/*
 * Copyright 2018 GoldenRiver Technologies Co., Ltd. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#ifndef _TRUSTY_VIRQ_H_
#define _TRUSTY_VIRQ_H_

#include <microvisor/microvisor.h>
#include <asm-generic/okl4_virq.h>

struct virq_data {
	wait_queue_head_t wq;
	int irqno;
	int hwirq;
	bool raised;
	unsigned long payload;
};

struct source_data {
	okl4_kcap_t kcap;
};

struct virq_device {
	struct device *dev;
	struct virq_data virq; /* notify from hee */
	struct source_data source; /* notify to hee */
#ifdef CONFIG_TRUSTY_VIRQ_SIMULATION
	void __iomem *gicd_base;
#endif
};

#ifdef CONFIG_TRUSTY_VIRQ_SIMULATION
/* Enabled only in QEMU environment. */
extern int gic_set_affinity_possible(struct irq_data *d,
		const struct cpumask *mask_val,
		bool force);
#endif

s32 trusty_virq_recv(u32 virqidx, ulong *out);
s32 trusty_virq_send(u32 virqidx, ulong payload);
s32 trusty_virq_smc(u32 virqidx, ulong r0, ulong r1, ulong r2, ulong r3);
struct virq_device *trusty_get_virq_device(u32 virqidx);

#endif /* _TRUSTY_VIRQ_H_ */
