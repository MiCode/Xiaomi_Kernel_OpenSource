/*
 * drivers/video/tegra/host/gk20a/gk20a.h
 *
 * GK20A Graphics
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef _NVHOST_GK20A_H_
#define _NVHOST_GK20A_H_


struct gk20a;
struct fifo_gk20a;
struct channel_gk20a;
struct gr_gk20a;
struct sim_gk20a;

#include <linux/tegra-soc.h>
#include <linux/spinlock.h>
#include <linux/nvhost_gpu_ioctl.h>
#include "clk_gk20a.h"
#include "fifo_gk20a.h"
#include "gr_gk20a.h"
#include "sim_gk20a.h"
#include "intr_gk20a.h"
#include "pmu_gk20a.h"
#include "priv_ring_gk20a.h"
#include "therm_gk20a.h"

#include "../../../../../arch/arm/mach-tegra/iomap.h"

extern struct platform_device tegra_gk20a_device;
extern struct nvhost_device_data tegra_gk20a_info;

static inline bool is_gk20a_module(struct platform_device *dev)
{
	return &tegra_gk20a_info == nvhost_get_devdata(dev);
}

struct cooling_device_gk20a {
	struct thermal_cooling_device *gk20a_cooling_dev;
	unsigned int gk20a_freq_state;
	unsigned int gk20a_freq_table_size;
	struct gk20a *g;
};

struct gk20a {
	struct nvhost_master *host;
	struct platform_device *dev;

	struct resource *reg_mem;
	void __iomem *regs;

	struct resource *bar1_mem;
	void __iomem *bar1;

	bool power_on;
	bool irq_requested;

	struct clk_gk20a clk;
	struct fifo_gk20a fifo;
	struct gr_gk20a gr;
	struct sim_gk20a sim;
	struct mm_gk20a mm;
	struct pmu_gk20a pmu;
	struct cooling_device_gk20a gk20a_cdev;

	/* Save pmu fw here so that it lives cross suspend/resume.
	   pmu suspend destroys all pmu sw/hw states. Loading pmu
	   fw in resume crashes when the resume is from sys_exit. */
	const struct firmware *pmu_fw;

	u32 gr_idle_timeout_default;
	u32 timeouts_enabled;

	bool slcg_enabled;
	bool blcg_enabled;
	bool elcg_enabled;
	bool elpg_enabled;
	bool aelpg_enabled;

#ifdef CONFIG_DEBUG_FS
	spinlock_t debugfs_lock;
	struct dentry *debugfs_ltc_enabled;
	struct dentry *debugfs_timeouts_enabled;
	struct dentry *debugfs_gr_idle_timeout_default;
#endif
	struct gk20a_ctxsw_ucode_info ctxsw_ucode_info;

	/* held while manipulating # of debug/profiler sessions present */
	/* also prevents debug sessions from attaching until released */
	struct mutex dbg_sessions_lock;
	int dbg_sessions; /* number attached */
	int dbg_powergating_disabled_refcount; /*refcount for pg disable */

	void (*remove_support)(struct platform_device *);

	u64 pg_ingating_time_us;
	u64 pg_ungating_time_us;
	u32 pg_gating_cnt;

	spinlock_t mc_enable_lock;

	struct nvhost_gpu_characteristics gpu_characteristics;
};

static inline unsigned long gk20a_get_gr_idle_timeout(struct gk20a *g)
{
	return g->timeouts_enabled ?
		g->gr_idle_timeout_default : MAX_SCHEDULE_TIMEOUT;
}

static inline struct gk20a *get_gk20a(struct platform_device *dev)
{
	return (struct gk20a *)nvhost_get_private_data(dev);
}

enum BAR0_DEBUG_OPERATION {
	BARO_ZERO_NOP = 0,
	OP_END = 'DONE',
	BAR0_READ32 = '0R32',
	BAR0_WRITE32 = '0W32',
};

struct share_buffer_head {
	enum BAR0_DEBUG_OPERATION operation;
/* size of the operation item */
	u32 size;
	u32 completed;
	u32 failed;
	u64 context;
	u64 completion_callback;
};

struct gk20a_cyclestate_buffer_elem {
	struct share_buffer_head	head;
/* in */
	u64 p_data;
	u64 p_done;
	u32 offset_bar0;
	u16 first_bit;
	u16 last_bit;
/* out */
/* keep 64 bits to be consistent */
	u64 data;
};

extern const struct nvhost_as_moduleops tegra_gk20a_as_ops;

/* register accessors */
static inline void gk20a_writel(struct gk20a *g, u32 r, u32 v)
{
	nvhost_dbg(dbg_reg, " r=0x%x v=0x%x", r, v);
	writel(v, g->regs + r);
}
static inline u32 gk20a_readl(struct gk20a *g, u32 r)
{
	u32 v = readl(g->regs + r);
	nvhost_dbg(dbg_reg, " r=0x%x v=0x%x", r, v);
	return v;
}

static inline void gk20a_bar1_writel(struct gk20a *g, u32 b, u32 v)
{
	nvhost_dbg(dbg_reg, " b=0x%x v=0x%x", b, v);
	writel(v, g->bar1 + b);
}

static inline u32 gk20a_bar1_readl(struct gk20a *g, u32 b)
{
	u32 v = readl(g->bar1 + b);
	nvhost_dbg(dbg_reg, " b=0x%x v=0x%x", b, v);
	return v;
}

/* convenience */
static inline struct device *dev_from_gk20a(struct gk20a *g)
{
	return &g->dev->dev;
}
static inline struct nvhost_syncpt *syncpt_from_gk20a(struct gk20a* g)
{
	return &(nvhost_get_host(g->dev)->syncpt);
}
static inline struct mem_mgr *mem_mgr_from_g(struct gk20a* g)
{
	return nvhost_get_host(g->dev)->memmgr;
}

static inline u32 u64_hi32(u64 n)
{
	return (u32)((n >> 32) & ~(u32)0);
}

static inline u32 u64_lo32(u64 n)
{
	return (u32)(n & ~(u32)0);
}

static inline u32 set_field(u32 val, u32 mask, u32 field)
{
	return ((val & ~mask) | field);
}

/* invalidate channel lookup tlb */
static inline void gk20a_gr_flush_channel_tlb(struct gr_gk20a *gr)
{
	spin_lock(&gr->ch_tlb_lock);
	memset(gr->chid_tlb, 0,
		sizeof(struct gr_channel_map_tlb_entry) *
		GR_CHANNEL_MAP_TLB_SIZE);
	spin_unlock(&gr->ch_tlb_lock);
}

/* This function can be called from two places, whichever comes first.
 * 1. nvhost calls this for gk20a driver init when client opens first gk20a channel.
 * 2. client opens gk20a ctrl node.
 */
int nvhost_gk20a_init(struct platform_device *dev);
void nvhost_gk20a_deinit(struct platform_device *dev);

/* classes that the device supports */
/* TBD: get these from an open-sourced SDK? */
enum {
	KEPLER_C                  = 0xA297,
	FERMI_TWOD_A              = 0x902D,
	KEPLER_COMPUTE_A          = 0xA0C0,
	KEPLER_INLINE_TO_MEMORY_A = 0xA040,
	KEPLER_DMA_COPY_A         = 0xA0B5, /*not sure about this one*/
};

#if defined (CONFIG_TEGRA_GK20A_PMU)
static inline int support_gk20a_pmu(void)
{
	return 1;
}
#else
static inline int support_gk20a_pmu(void){return 0;}
#endif

int nvhost_gk20a_finalize_poweron(struct platform_device *dev);
int nvhost_gk20a_prepare_poweroff(struct platform_device *dev);
void nvhost_gk20a_scale_notify_idle(struct platform_device *pdev);
void nvhost_gk20a_scale_notify_busy(struct platform_device *pdev);
void nvhost_gk20a_scale_init(struct platform_device *pdev);
void nvhost_gk20a_scale_deinit(struct platform_device *pdev);

void gk20a_create_sysfs(struct platform_device *dev);

#ifdef CONFIG_DEBUG_FS
int clk_gk20a_debugfs_init(struct platform_device *dev);
#endif

extern const struct file_operations tegra_gk20a_ctrl_ops;
extern const struct file_operations tegra_gk20a_dbg_gpu_ops;
extern const struct file_operations tegra_gk20a_prof_gpu_ops;

struct nvhost_hwctx_handler *nvhost_gk20a_alloc_hwctx_handler(u32 syncpt,
		u32 waitbase, struct nvhost_channel *ch);

#define GK20A_BAR0_IORESOURCE_MEM 0
#define GK20A_BAR1_IORESOURCE_MEM 1
#define GK20A_SIM_IORESOURCE_MEM 2
#define TEGRA_GK20A_SIM_BASE 0x538F0000 /*tbd: get from iomap.h */
#define TEGRA_GK20A_SIM_SIZE 0x1000     /*tbd: this is a high-side guess */

void gk20a_busy(struct platform_device *pdev);
void gk20a_idle(struct platform_device *pdev);
void gk20a_disable(struct gk20a *g, u32 units);
void gk20a_enable(struct gk20a *g, u32 units);
void gk20a_reset(struct gk20a *g, u32 units);

#define NVHOST_GPU_ARCHITECTURE_SHIFT 4

/* constructs unique and compact GPUID from nvhost_gpu_characteristics
 * arch/impl fields */
#define GK20A_GPUID(arch, impl) ((u32) ((arch) | (impl)))

#define GK20A_GPUID_GK20A \
	GK20A_GPUID(NVHOST_GPU_ARCH_GK100, NVHOST_GPU_IMPL_GK20A)

int gk20a_init_gpu_characteristics(struct gk20a *g);

#endif /* _NVHOST_GK20A_H_ */
