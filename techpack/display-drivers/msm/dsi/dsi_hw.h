/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _DSI_HW_H_
#define _DSI_HW_H_
#include <linux/io.h>

#define DSI_GEN_R32(base, offset) readl_relaxed((base) + (offset))
#define DSI_GEN_W32(base, offset, val) writel_relaxed((val), (base) + (offset))

#define DSI_R64(dsi_hw, off) readq_relaxed((dsi_hw)->base + (off))
#define DSI_W64(dsi_hw, off, val) writeq_relaxed((val), (dsi_hw)->base + (off))

#define DSI_READ_POLL_TIMEOUT(dsi_hw, off, val, cond, delay_us, timeout_us) \
	readl_poll_timeout((dsi_hw)->base + (off), (val), (cond), (delay_us), (timeout_us))

#define DSI_READ_POLL_TIMEOUT_ATOMIC_GEN(base, index, off, val, cond, delay_us, timeout_us) \
	readl_poll_timeout_atomic((base) + (off), (val), (cond), (delay_us), (timeout_us))

#define DSI_GEN_W32_DEBUG(base, index, offset, val) \
	do {\
		pr_debug("[DSI_%d][%s] - [0x%08x]\n", \
			(index), #offset, (uint32_t)(val)); \
		DSI_GEN_W32(base, offset, val); \
	} while (0)

#define DSI_R32(dsi_hw, off) DSI_GEN_R32((dsi_hw)->base, off)
#define DSI_W32(dsi_hw, off, val) DSI_GEN_W32_DEBUG((dsi_hw)->base,	\
	(dsi_hw)->index, off, val)

#define DSI_READ_POLL_TIMEOUT_ATOMIC(dsi_hw, off, val, cond, delay_us, timeout_us) \
	DSI_READ_POLL_TIMEOUT_ATOMIC_GEN((dsi_hw)->base, (dsi_hw)->index, off, val, cond, delay_us, timeout_us)

#define PLL_CALC_DATA(addr0, addr1, data0, data1)      \
	(((data1) << 24) | ((((addr1)/4) & 0xFF) << 16) | \
	 ((data0) << 8) | (((addr0)/4) & 0xFF))

#define DSI_DYN_REF_REG_W(base, offset, addr0, addr1, data0, data1)   \
	DSI_GEN_W32(base, offset, PLL_CALC_DATA(addr0, addr1, data0, data1))
#endif /* _DSI_HW_H_ */
