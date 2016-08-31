/*
 * drivers/video/tegra/host/dev.h
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NVHOST_DEV_H
#define NVHOST_DEV_H

#include "host1x/host1x.h"

struct platform_device;

void nvhost_device_list_init(void);
int nvhost_device_list_add(struct platform_device *pdev);
void nvhost_device_list_for_all(void *data,
	int (*fptr)(struct platform_device *pdev, void *fdata, int locked_id),
	int locked_id);
struct platform_device *nvhost_device_list_match_by_id(u32 id);
void nvhost_device_list_remove(struct platform_device *pdev);


#ifdef CONFIG_DEBUG_FS
    /* debug info, default is compiled-in but effectively disabled (0 mask) */
    #define NVHOST_DEBUG
    /*e.g: echo 1 > /d/tegra_host/dbg_mask */
    #define NVHOST_DEFAULT_DBG_MASK 0
#else
    /* manually enable and turn it on the mask */
    /*#define NVHOST_DEBUG*/
    #define NVHOST_DEFAULT_DBG_MASK (dbg_info)
#endif

enum nvhost_dbg_categories {
	dbg_info    = BIT(0),  /* lightly verbose info */
	dbg_fn      = BIT(2),  /* fn name tracing */
	dbg_reg     = BIT(3),  /* register accesses, very verbose */
	dbg_pte     = BIT(4),  /* gmmu ptes */
	dbg_intr    = BIT(5),  /* interrupts */
	dbg_pmu     = BIT(6),  /* gk20a pmu */
	dbg_clk     = BIT(7),  /* gk20a clk */
	dbg_map     = BIT(8),  /* mem mappings */
	dbg_gpu_dbg  = BIT(9),  /* gpu debugger/profiler */
	dbg_mem     = BIT(31), /* memory accesses, very verbose */
};

#if defined(NVHOST_DEBUG)
extern u32 nvhost_dbg_mask;
extern u32 nvhost_dbg_ftrace;
#define nvhost_dbg(dbg_mask, format, arg...)				\
do {									\
	if (unlikely((dbg_mask) & nvhost_dbg_mask)) {			\
		if (nvhost_dbg_ftrace)					\
			trace_printk(format "\n", ##arg);		\
		else							\
			pr_info("nvhost %s: " format "\n",		\
					__func__, ##arg);		\
	}								\
} while (0)

#else /* NVHOST_DEBUG */
#define nvhost_dbg(dbg_mask, format, arg...)				\
do {									\
	if (0)								\
		printk(KERN_INFO "nvhost %s: " format "\n", __func__, ##arg);\
} while (0)

#endif


/* convenience,shorter err/fn/dbg_info */
#define nvhost_err(d, fmt, arg...) \
	dev_err(d, "%s: " fmt "\n", __func__, ##arg)

#define nvhost_warn(d, fmt, arg...) \
	dev_warn(d, "%s: " fmt "\n", __func__, ##arg)

#define nvhost_dbg_fn(fmt, arg...) \
	nvhost_dbg(dbg_fn, fmt, ##arg)

#define nvhost_dbg_info(fmt, arg...) \
	nvhost_dbg(dbg_info, fmt, ##arg)

/* mem access with dbg_mem logging */
static inline u8 mem_rd08(void *ptr, int b)
{
	u8 _b = ((const u8 *)ptr)[b];
#ifdef CONFIG_TEGRA_SIMULATION_PLATFORM
	nvhost_dbg(dbg_mem, " %p = 0x%x", ptr+sizeof(u8)*b, _b);
#endif
	return _b;
}
static inline u16 mem_rd16(void *ptr, int s)
{
	u16 _s = ((const u16 *)ptr)[s];
#ifdef CONFIG_TEGRA_SIMULATION_PLATFORM
	nvhost_dbg(dbg_mem, " %p = 0x%x", ptr+sizeof(u16)*s, _s);
#endif
	return _s;
}
static inline u32 mem_rd32(void *ptr, int w)
{
	u32 _w = ((const u32 *)ptr)[w];
#ifdef CONFIG_TEGRA_SIMULATION_PLATFORM
	nvhost_dbg(dbg_mem, " %p = 0x%x", ptr + sizeof(u32)*w, _w);
#endif
	return _w;
}
static inline void mem_wr08(void *ptr, int b, u8 data)
{
#ifdef CONFIG_TEGRA_SIMULATION_PLATFORM
	nvhost_dbg(dbg_mem, " %p = 0x%x", ptr+sizeof(u8)*b, data);
#endif
	((u8 *)ptr)[b] = data;
}
static inline void mem_wr16(void *ptr, int s, u16 data)
{
#ifdef CONFIG_TEGRA_SIMULATION_PLATFORM
	nvhost_dbg(dbg_mem, " %p = 0x%x", ptr+sizeof(u16)*s, data);
#endif
	((u16 *)ptr)[s] = data;
}
static inline void mem_wr32(void *ptr, int w, u32 data)
{
#ifdef CONFIG_TEGRA_SIMULATION_PLATFORM
	nvhost_dbg(dbg_mem, " %p = 0x%x", ptr+sizeof(u32)*w, data);
#endif
	((u32 *)ptr)[w] = data;
}

static inline u32 bit_mask(u32 nr)
{
	return 1UL << (nr % 32);
}

static inline u32 bit_word(u32 nr)
{
	return nr / 32;
}
#endif
