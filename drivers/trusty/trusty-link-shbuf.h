/*
 * Copyright 2018 GoldenRiver Technologies Co., Ltd. All rights reserved.
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

#ifndef _TRUSTY_LINK_SHBUF_H_
#define _TRUSTY_LINK_SHBUF_H_

#include <microvisor/microvisor.h>
#include <asm-generic/okl4_virq.h>

/* Private data for this driver */
struct link_shbuf_data {

	/* Outgoing vIRQ */
	u32 virqline;

	/* Incoming vIRQ */
	int virq;
	int hwirq;
	atomic64_t virq_payload;
	bool virq_pending;
	wait_queue_head_t virq_wq;

	/* Shared memory region */
	void *base;
	fmode_t permissions;
	struct resource buffer;
	size_t ramconsole_size;

	struct device *dev;
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

s32 trusty_link_shbuf_recv(u32 lsdidx, ulong *out);
s32 trusty_link_shbuf_send(u32 lsdidx, ulong payload);
s32 trusty_link_shbuf_smc(u32 lsdidx, ulong r0, ulong r1, ulong r2, ulong r3);
struct link_shbuf_data *trusty_get_link_shbuf_device(u32 lsdidx);

#endif /* _TRUSTY_LINK_SHBUF_H_ */
