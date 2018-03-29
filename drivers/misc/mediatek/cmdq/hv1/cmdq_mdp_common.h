/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __CMDQ_MDP_COMMON_H__
#define __CMDQ_MDP_COMMON_H__

#include "cmdq_def.h"

#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif


/* #include <mach/mt_clkmgr.h> */

	void cmdq_mdp_enable(uint64_t engineFlag,
			     CMDQ_CLK_ENUM gateId, enum CMDQ_ENG_ENUM engine, const char *name);
	/* enum cg_clk_id gateId, CMDQ_ENG_ENUM engine, const char *name); */
	/* CCF */

	/* CCF */
	/* int cmdq_mdp_loop_reset(enum cg_clk_id clkId, */
	int cmdq_mdp_loop_reset(enum CMDQ_CLK_ENUM clkId,
				const unsigned long resetReg,
				const unsigned long resetStateReg,
				const uint32_t resetMask,
				const uint32_t resetValue, const char *name,
				const bool pollInitResult);

	/* CCF */
	/* void cmdq_mdp_loop_off(enum cg_clk_id clkId, */
	void cmdq_mdp_loop_off(enum CMDQ_CLK_ENUM clkId,
			       const unsigned long resetReg,
			       const unsigned long resetStateReg,
			       const uint32_t resetMask,
			       const uint32_t resetValue, const char *name,
			       const bool pollInitResult);

	void cmdq_mdp_dump_venc(const unsigned long base, const char *label);
	void cmdq_mdp_dump_rdma(const unsigned long base, const char *label);
	void cmdq_mdp_dump_rsz(const unsigned long base, const char *label);
	void cmdq_mdp_dump_rot(const unsigned long base, const char *label);
	void cmdq_mdp_dump_tdshp(const unsigned long base, const char *label);
	void cmdq_mdp_dump_wdma(const unsigned long base, const char *label);

#ifdef __cplusplus
}
#endif
#endif				/* __CMDQ_MDP_PLATFORM_H__ */
