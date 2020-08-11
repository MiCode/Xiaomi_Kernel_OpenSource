/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _DSI_HW_H_
#define _DSI_HW_H_
#include <linux/io.h>

#define DSI_R32(dsi_hw, off) readl_relaxed((dsi_hw)->base + (off))
#define DSI_W32(dsi_hw, off, val) \
	do {\
		pr_debug("[DSI_%d][%s] - [0x%08x]\n", \
			(dsi_hw)->index, #off, (uint32_t)(val)); \
		writel_relaxed((val), (dsi_hw)->base + (off)); \
	} while (0)

#define DSI_MMSS_MISC_R32(dsi_hw, off) \
	readl_relaxed((dsi_hw)->mmss_misc_base + (off))
#define DSI_MMSS_MISC_W32(dsi_hw, off, val) \
	do {\
		pr_debug("[DSI_%d][%s] - [0x%08x]\n", \
			(dsi_hw)->index, #off, val); \
		writel_relaxed((val), (dsi_hw)->mmss_misc_base + (off)); \
	} while (0)

#define DSI_MISC_R32(dsi_hw, off) \
	readl_relaxed((dsi_hw)->phy_clamp_base + (off))
#define DSI_MISC_W32(dsi_hw, off, val) \
	do {\
		pr_debug("[DSI_%d][%s] - [0x%08x]\n", \
			(dsi_hw)->index, #off, val); \
		writel_relaxed((val), (dsi_hw)->phy_clamp_base + (off)); \
	} while (0)
#define DSI_DISP_CC_R32(dsi_hw, off) \
	readl_relaxed((dsi_hw)->disp_cc_base + (off))
#define DSI_DISP_CC_W32(dsi_hw, off, val) \
	do {\
		pr_debug("[DSI_%d][%s] - [0x%08x]\n", \
			(dsi_hw)->index, #off, val); \
		writel_relaxed((val), (dsi_hw)->disp_cc_base + (off)); \
	} while (0)

#define DSI_R64(dsi_hw, off) readq_relaxed((dsi_hw)->base + (off))
#define DSI_W64(dsi_hw, off, val) writeq_relaxed((val), (dsi_hw)->base + (off))

#define PLL_CALC_DATA(addr0, addr1, data0, data1)      \
	(((data1) << 24) | ((((addr1)/4) & 0xFF) << 16) | \
	 ((data0) << 8) | (((addr0)/4) & 0xFF))

#define DSI_DYN_REF_REG_W(base, offset, addr0, addr1, data0, data1)   \
	writel_relaxed(PLL_CALC_DATA(addr0, addr1, data0, data1), \
			(base) + (offset))

#define DSI_GEN_R32(base, offset) readl_relaxed(base + (offset))
#define DSI_GEN_W32(base, offset, val) writel_relaxed((val), base + (offset))
#endif /* _DSI_HW_H_ */
