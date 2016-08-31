/*
 * drivers/video/tegra/host/gk20a/sim_gk20a.h
 *
 * GK20A sim support
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef __SIM_GK20A_H__
#define __SIM_GK20A_H__


struct gk20a;
struct sim_gk20a {
	struct gk20a *g;
	struct resource *reg_mem;
	void __iomem *regs;
	struct {
		struct page *page;
		void *kvaddr;
		phys_addr_t phys;
	} send_bfr, recv_bfr, msg_bfr;
	u32 send_ring_put;
	u32 recv_ring_get;
	u32 recv_ring_put;
	u32 sequence_base;
	void (*remove_support)(struct sim_gk20a *);
};


int gk20a_sim_esc_read(struct gk20a *g, char *path, u32 index,
			  u32 count, u32 *data);

static inline int gk20a_sim_esc_read_no_sim(struct gk20a *g, char *p,
				     u32 i, u32 c, u32 *d)
{
	*d = ~(u32)0;
	return -1;
}

static inline int gk20a_sim_esc_readl(struct gk20a *g, char * p, u32 i, u32 *d)
{
	if (tegra_cpu_is_asim())
		return gk20a_sim_esc_read(g, p, i, sizeof(u32), d);

	return gk20a_sim_esc_read_no_sim(g, p, i, sizeof(u32), d);
}


#endif /*__SIM_GK20A_H__*/
