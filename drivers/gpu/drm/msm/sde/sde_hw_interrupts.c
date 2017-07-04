/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#include <linux/bitops.h>
#include <linux/slab.h>

#include "sde_kms.h"
#include "sde_hw_interrupts.h"
#include "sde_hw_util.h"
#include "sde_hw_mdss.h"

/**
 * Register offsets in MDSS register file for the interrupt registers
 * w.r.t. to the MDSS base
 */
#define HW_INTR_STATUS			0x0010
#define MDP_SSPP_TOP0_OFF		0x1000
#define MDP_INTF_0_OFF			0x6B000
#define MDP_INTF_1_OFF			0x6B800
#define MDP_INTF_2_OFF			0x6C000
#define MDP_INTF_3_OFF			0x6C800
#define MDP_INTF_4_OFF			0x6D000

/**
 * WB interrupt status bit definitions
 */
#define SDE_INTR_WB_0_DONE BIT(0)
#define SDE_INTR_WB_1_DONE BIT(1)
#define SDE_INTR_WB_2_DONE BIT(4)

/**
 * WDOG timer interrupt status bit definitions
 */
#define SDE_INTR_WD_TIMER_0_DONE BIT(2)
#define SDE_INTR_WD_TIMER_1_DONE BIT(3)
#define SDE_INTR_WD_TIMER_2_DONE BIT(5)
#define SDE_INTR_WD_TIMER_3_DONE BIT(6)
#define SDE_INTR_WD_TIMER_4_DONE BIT(7)

/**
 * Pingpong interrupt status bit definitions
 */
#define SDE_INTR_PING_PONG_0_DONE BIT(8)
#define SDE_INTR_PING_PONG_1_DONE BIT(9)
#define SDE_INTR_PING_PONG_2_DONE BIT(10)
#define SDE_INTR_PING_PONG_3_DONE BIT(11)
#define SDE_INTR_PING_PONG_0_RD_PTR BIT(12)
#define SDE_INTR_PING_PONG_1_RD_PTR BIT(13)
#define SDE_INTR_PING_PONG_2_RD_PTR BIT(14)
#define SDE_INTR_PING_PONG_3_RD_PTR BIT(15)
#define SDE_INTR_PING_PONG_0_WR_PTR BIT(16)
#define SDE_INTR_PING_PONG_1_WR_PTR BIT(17)
#define SDE_INTR_PING_PONG_2_WR_PTR BIT(18)
#define SDE_INTR_PING_PONG_3_WR_PTR BIT(19)
#define SDE_INTR_PING_PONG_0_AUTOREFRESH_DONE BIT(20)
#define SDE_INTR_PING_PONG_1_AUTOREFRESH_DONE BIT(21)
#define SDE_INTR_PING_PONG_2_AUTOREFRESH_DONE BIT(22)
#define SDE_INTR_PING_PONG_3_AUTOREFRESH_DONE BIT(23)

/**
 * Interface interrupt status bit definitions
 */
#define SDE_INTR_INTF_0_UNDERRUN BIT(24)
#define SDE_INTR_INTF_1_UNDERRUN BIT(26)
#define SDE_INTR_INTF_2_UNDERRUN BIT(28)
#define SDE_INTR_INTF_3_UNDERRUN BIT(30)
#define SDE_INTR_INTF_0_VSYNC BIT(25)
#define SDE_INTR_INTF_1_VSYNC BIT(27)
#define SDE_INTR_INTF_2_VSYNC BIT(29)
#define SDE_INTR_INTF_3_VSYNC BIT(31)

/**
 * Pingpong Secondary interrupt status bit definitions
 */
#define SDE_INTR_PING_PONG_S0_AUTOREFRESH_DONE BIT(0)
#define	SDE_INTR_PING_PONG_S0_WR_PTR BIT(4)
#define SDE_INTR_PING_PONG_S0_RD_PTR BIT(8)
#define SDE_INTR_PING_PONG_S0_TEAR_DETECTED BIT(22)
#define SDE_INTR_PING_PONG_S0_TE_DETECTED BIT(28)

/**
 * Pingpong TEAR detection interrupt status bit definitions
 */
#define SDE_INTR_PING_PONG_0_TEAR_DETECTED BIT(16)
#define SDE_INTR_PING_PONG_1_TEAR_DETECTED BIT(17)
#define SDE_INTR_PING_PONG_2_TEAR_DETECTED BIT(18)
#define SDE_INTR_PING_PONG_3_TEAR_DETECTED BIT(19)

/**
 * Pingpong TE detection interrupt status bit definitions
 */
#define SDE_INTR_PING_PONG_0_TE_DETECTED BIT(24)
#define SDE_INTR_PING_PONG_1_TE_DETECTED BIT(25)
#define SDE_INTR_PING_PONG_2_TE_DETECTED BIT(26)
#define SDE_INTR_PING_PONG_3_TE_DETECTED BIT(27)

/**
 * Concurrent WB overflow interrupt status bit definitions
 */
#define SDE_INTR_CWB_2_OVERFLOW BIT(14)
#define SDE_INTR_CWB_3_OVERFLOW BIT(15)

/**
 * Histogram VIG done interrupt status bit definitions
 */
#define SDE_INTR_HIST_VIG_0_DONE BIT(0)
#define SDE_INTR_HIST_VIG_1_DONE BIT(4)
#define SDE_INTR_HIST_VIG_2_DONE BIT(8)
#define SDE_INTR_HIST_VIG_3_DONE BIT(10)

/**
 * Histogram VIG reset Sequence done interrupt status bit definitions
 */
#define SDE_INTR_HIST_VIG_0_RSTSEQ_DONE BIT(1)
#define SDE_INTR_HIST_VIG_1_RSTSEQ_DONE BIT(5)
#define SDE_INTR_HIST_VIG_2_RSTSEQ_DONE BIT(9)
#define SDE_INTR_HIST_VIG_3_RSTSEQ_DONE BIT(11)

/**
 * Histogram DSPP done interrupt status bit definitions
 */
#define SDE_INTR_HIST_DSPP_0_DONE BIT(12)
#define SDE_INTR_HIST_DSPP_1_DONE BIT(16)
#define SDE_INTR_HIST_DSPP_2_DONE BIT(20)
#define SDE_INTR_HIST_DSPP_3_DONE BIT(22)

/**
 * Histogram DSPP reset Sequence done interrupt status bit definitions
 */
#define SDE_INTR_HIST_DSPP_0_RSTSEQ_DONE BIT(13)
#define SDE_INTR_HIST_DSPP_1_RSTSEQ_DONE BIT(17)
#define SDE_INTR_HIST_DSPP_2_RSTSEQ_DONE BIT(21)
#define SDE_INTR_HIST_DSPP_3_RSTSEQ_DONE BIT(23)

/**
 * INTF interrupt status bit definitions
 */
#define SDE_INTR_VIDEO_INTO_STATIC BIT(0)
#define SDE_INTR_VIDEO_OUTOF_STATIC BIT(1)
#define SDE_INTR_DSICMD_0_INTO_STATIC BIT(2)
#define SDE_INTR_DSICMD_0_OUTOF_STATIC BIT(3)
#define SDE_INTR_DSICMD_1_INTO_STATIC BIT(4)
#define SDE_INTR_DSICMD_1_OUTOF_STATIC BIT(5)
#define SDE_INTR_DSICMD_2_INTO_STATIC BIT(6)
#define SDE_INTR_DSICMD_2_OUTOF_STATIC BIT(7)
#define SDE_INTR_PROG_LINE BIT(8)

/**
 * struct sde_intr_reg - array of SDE register sets
 * @clr_off:	offset to CLEAR reg
 * @en_off:	offset to ENABLE reg
 * @status_off:	offset to STATUS reg
 */
struct sde_intr_reg {
	u32 clr_off;
	u32 en_off;
	u32 status_off;
};

/**
 * struct sde_irq_type - maps each irq with i/f
 * @intr_type:		type of interrupt listed in sde_intr_type
 * @instance_idx:	instance index of the associated HW block in SDE
 * @irq_mask:		corresponding bit in the interrupt status reg
 * @reg_idx:		which reg set to use
 */
struct sde_irq_type {
	u32 intr_type;
	u32 instance_idx;
	u32 irq_mask;
	u32 reg_idx;
};

/**
 * List of SDE interrupt registers
 */
static const struct sde_intr_reg sde_intr_set[] = {
	{
		MDP_SSPP_TOP0_OFF+INTR_CLEAR,
		MDP_SSPP_TOP0_OFF+INTR_EN,
		MDP_SSPP_TOP0_OFF+INTR_STATUS
	},
	{
		MDP_SSPP_TOP0_OFF+INTR2_CLEAR,
		MDP_SSPP_TOP0_OFF+INTR2_EN,
		MDP_SSPP_TOP0_OFF+INTR2_STATUS
	},
	{
		MDP_SSPP_TOP0_OFF+HIST_INTR_CLEAR,
		MDP_SSPP_TOP0_OFF+HIST_INTR_EN,
		MDP_SSPP_TOP0_OFF+HIST_INTR_STATUS
	},
	{
		MDP_INTF_0_OFF+INTF_INTR_CLEAR,
		MDP_INTF_0_OFF+INTF_INTR_EN,
		MDP_INTF_0_OFF+INTF_INTR_STATUS
	},
	{
		MDP_INTF_1_OFF+INTF_INTR_CLEAR,
		MDP_INTF_1_OFF+INTF_INTR_EN,
		MDP_INTF_1_OFF+INTF_INTR_STATUS
	},
	{
		MDP_INTF_2_OFF+INTF_INTR_CLEAR,
		MDP_INTF_2_OFF+INTF_INTR_EN,
		MDP_INTF_2_OFF+INTF_INTR_STATUS
	},
	{
		MDP_INTF_3_OFF+INTF_INTR_CLEAR,
		MDP_INTF_3_OFF+INTF_INTR_EN,
		MDP_INTF_3_OFF+INTF_INTR_STATUS
	},
	{
		MDP_INTF_4_OFF+INTF_INTR_CLEAR,
		MDP_INTF_4_OFF+INTF_INTR_EN,
		MDP_INTF_4_OFF+INTF_INTR_STATUS
	}
};

/**
 * IRQ mapping table - use for lookup an irq_idx in this table that have
 *                     a matching interface type and instance index.
 */
static const struct sde_irq_type sde_irq_map[] = {
	/* BEGIN MAP_RANGE: 0-31, INTR */
	/* irq_idx: 0-3 */
	{ SDE_IRQ_TYPE_WB_ROT_COMP, WB_0, SDE_INTR_WB_0_DONE, 0},
	{ SDE_IRQ_TYPE_WB_ROT_COMP, WB_1, SDE_INTR_WB_1_DONE, 0},
	{ SDE_IRQ_TYPE_WD_TIMER, WD_TIMER_0, SDE_INTR_WD_TIMER_0_DONE, 0},
	{ SDE_IRQ_TYPE_WD_TIMER, WD_TIMER_1, SDE_INTR_WD_TIMER_1_DONE, 0},
	/* irq_idx: 4-7 */
	{ SDE_IRQ_TYPE_WB_WFD_COMP, WB_2, SDE_INTR_WB_2_DONE, 0},
	{ SDE_IRQ_TYPE_WD_TIMER, WD_TIMER_2, SDE_INTR_WD_TIMER_2_DONE, 0},
	{ SDE_IRQ_TYPE_WD_TIMER, WD_TIMER_3, SDE_INTR_WD_TIMER_3_DONE, 0},
	{ SDE_IRQ_TYPE_WD_TIMER, WD_TIMER_4, SDE_INTR_WD_TIMER_4_DONE, 0},
	/* irq_idx: 8-11 */
	{ SDE_IRQ_TYPE_PING_PONG_COMP, PINGPONG_0,
		SDE_INTR_PING_PONG_0_DONE, 0},
	{ SDE_IRQ_TYPE_PING_PONG_COMP, PINGPONG_1,
		SDE_INTR_PING_PONG_1_DONE, 0},
	{ SDE_IRQ_TYPE_PING_PONG_COMP, PINGPONG_2,
		SDE_INTR_PING_PONG_2_DONE, 0},
	{ SDE_IRQ_TYPE_PING_PONG_COMP, PINGPONG_3,
		SDE_INTR_PING_PONG_3_DONE, 0},
	/* irq_idx: 12-15 */
	{ SDE_IRQ_TYPE_PING_PONG_RD_PTR, PINGPONG_0,
		SDE_INTR_PING_PONG_0_RD_PTR, 0},
	{ SDE_IRQ_TYPE_PING_PONG_RD_PTR, PINGPONG_1,
		SDE_INTR_PING_PONG_1_RD_PTR, 0},
	{ SDE_IRQ_TYPE_PING_PONG_RD_PTR, PINGPONG_2,
		SDE_INTR_PING_PONG_2_RD_PTR, 0},
	{ SDE_IRQ_TYPE_PING_PONG_RD_PTR, PINGPONG_3,
		SDE_INTR_PING_PONG_3_RD_PTR, 0},
	/* irq_idx: 16-19 */
	{ SDE_IRQ_TYPE_PING_PONG_WR_PTR, PINGPONG_0,
		SDE_INTR_PING_PONG_0_WR_PTR, 0},
	{ SDE_IRQ_TYPE_PING_PONG_WR_PTR, PINGPONG_1,
		SDE_INTR_PING_PONG_1_WR_PTR, 0},
	{ SDE_IRQ_TYPE_PING_PONG_WR_PTR, PINGPONG_2,
		SDE_INTR_PING_PONG_2_WR_PTR, 0},
	{ SDE_IRQ_TYPE_PING_PONG_WR_PTR, PINGPONG_3,
		SDE_INTR_PING_PONG_3_WR_PTR, 0},
	/* irq_idx: 20-23 */
	{ SDE_IRQ_TYPE_PING_PONG_AUTO_REF, PINGPONG_0,
		SDE_INTR_PING_PONG_0_AUTOREFRESH_DONE, 0},
	{ SDE_IRQ_TYPE_PING_PONG_AUTO_REF, PINGPONG_1,
		SDE_INTR_PING_PONG_1_AUTOREFRESH_DONE, 0},
	{ SDE_IRQ_TYPE_PING_PONG_AUTO_REF, PINGPONG_2,
		SDE_INTR_PING_PONG_2_AUTOREFRESH_DONE, 0},
	{ SDE_IRQ_TYPE_PING_PONG_AUTO_REF, PINGPONG_3,
		SDE_INTR_PING_PONG_3_AUTOREFRESH_DONE, 0},
	/* irq_idx: 24-27 */
	{ SDE_IRQ_TYPE_INTF_UNDER_RUN, INTF_0, SDE_INTR_INTF_0_UNDERRUN, 0},
	{ SDE_IRQ_TYPE_INTF_VSYNC, INTF_0, SDE_INTR_INTF_0_VSYNC, 0},
	{ SDE_IRQ_TYPE_INTF_UNDER_RUN, INTF_1, SDE_INTR_INTF_1_UNDERRUN, 0},
	{ SDE_IRQ_TYPE_INTF_VSYNC, INTF_1, SDE_INTR_INTF_1_VSYNC, 0},
	/* irq_idx: 28-31 */
	{ SDE_IRQ_TYPE_INTF_UNDER_RUN, INTF_2, SDE_INTR_INTF_2_UNDERRUN, 0},
	{ SDE_IRQ_TYPE_INTF_VSYNC, INTF_2, SDE_INTR_INTF_2_VSYNC, 0},
	{ SDE_IRQ_TYPE_INTF_UNDER_RUN, INTF_3, SDE_INTR_INTF_3_UNDERRUN, 0},
	{ SDE_IRQ_TYPE_INTF_VSYNC, INTF_3, SDE_INTR_INTF_3_VSYNC, 0},

	/* BEGIN MAP_RANGE: 32-64, INTR2 */
	/* irq_idx: 32-35 */
	{ SDE_IRQ_TYPE_PING_PONG_AUTO_REF, PINGPONG_S0,
		SDE_INTR_PING_PONG_S0_AUTOREFRESH_DONE, 1},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 1},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 1},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 1},
	/* irq_idx: 36-39 */
	{ SDE_IRQ_TYPE_PING_PONG_WR_PTR, PINGPONG_S0,
		SDE_INTR_PING_PONG_S0_WR_PTR, 1},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 1},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 1},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 1},
	/* irq_idx: 40-43 */
	{ SDE_IRQ_TYPE_PING_PONG_RD_PTR, PINGPONG_S0,
		SDE_INTR_PING_PONG_S0_RD_PTR, 1},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 1},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 1},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 1},
	/* irq_idx: 44-47 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 1},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 1},
	{ SDE_IRQ_TYPE_CWB_OVERFLOW, CWB_2, SDE_INTR_CWB_2_OVERFLOW, 1},
	{ SDE_IRQ_TYPE_CWB_OVERFLOW, CWB_3, SDE_INTR_CWB_3_OVERFLOW, 1},
	/* irq_idx: 48-51 */
	{ SDE_IRQ_TYPE_PING_PONG_TEAR_CHECK, PINGPONG_0,
		SDE_INTR_PING_PONG_0_TEAR_DETECTED, 1},
	{ SDE_IRQ_TYPE_PING_PONG_TEAR_CHECK, PINGPONG_1,
		SDE_INTR_PING_PONG_1_TEAR_DETECTED, 1},
	{ SDE_IRQ_TYPE_PING_PONG_TEAR_CHECK, PINGPONG_2,
		SDE_INTR_PING_PONG_2_TEAR_DETECTED, 1},
	{ SDE_IRQ_TYPE_PING_PONG_TEAR_CHECK, PINGPONG_3,
		SDE_INTR_PING_PONG_3_TEAR_DETECTED, 1},
	/* irq_idx: 52-55 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 1},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 1},
	{ SDE_IRQ_TYPE_PING_PONG_TEAR_CHECK, PINGPONG_S0,
		SDE_INTR_PING_PONG_S0_TEAR_DETECTED, 1},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 1},
	/* irq_idx: 56-59 */
	{ SDE_IRQ_TYPE_PING_PONG_TE_CHECK, PINGPONG_0,
		SDE_INTR_PING_PONG_0_TE_DETECTED, 1},
	{ SDE_IRQ_TYPE_PING_PONG_TE_CHECK, PINGPONG_1,
		SDE_INTR_PING_PONG_1_TE_DETECTED, 1},
	{ SDE_IRQ_TYPE_PING_PONG_TE_CHECK, PINGPONG_2,
		SDE_INTR_PING_PONG_2_TE_DETECTED, 1},
	{ SDE_IRQ_TYPE_PING_PONG_TE_CHECK, PINGPONG_3,
		SDE_INTR_PING_PONG_3_TE_DETECTED, 1},
	/* irq_idx: 60-63 */
	{ SDE_IRQ_TYPE_PING_PONG_TE_CHECK, PINGPONG_S0,
		SDE_INTR_PING_PONG_S0_TE_DETECTED, 1},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 1},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 1},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 1},

	/* BEGIN MAP_RANGE: 64-95 HIST */
	/* irq_idx: 64-67 */
	{ SDE_IRQ_TYPE_HIST_VIG_DONE, SSPP_VIG0, SDE_INTR_HIST_VIG_0_DONE, 2},
	{ SDE_IRQ_TYPE_HIST_VIG_RSTSEQ, SSPP_VIG0,
		SDE_INTR_HIST_VIG_0_RSTSEQ_DONE, 2},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 2},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 2},
	/* irq_idx: 68-71 */
	{ SDE_IRQ_TYPE_HIST_VIG_DONE, SSPP_VIG1, SDE_INTR_HIST_VIG_1_DONE, 2},
	{ SDE_IRQ_TYPE_HIST_VIG_RSTSEQ, SSPP_VIG1,
		SDE_INTR_HIST_VIG_1_RSTSEQ_DONE, 2},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 2},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 2},
	/* irq_idx: 72-75 */
	{ SDE_IRQ_TYPE_HIST_VIG_DONE, SSPP_VIG2, SDE_INTR_HIST_VIG_2_DONE, 2},
	{ SDE_IRQ_TYPE_HIST_VIG_RSTSEQ, SSPP_VIG2,
		SDE_INTR_HIST_VIG_2_RSTSEQ_DONE, 2},
	{ SDE_IRQ_TYPE_HIST_VIG_DONE, SSPP_VIG3, SDE_INTR_HIST_VIG_3_DONE, 2},
	{ SDE_IRQ_TYPE_HIST_VIG_RSTSEQ, SSPP_VIG3,
		SDE_INTR_HIST_VIG_3_RSTSEQ_DONE, 2},
	/* irq_idx: 76-79 */
	{ SDE_IRQ_TYPE_HIST_DSPP_DONE, DSPP_0, SDE_INTR_HIST_DSPP_0_DONE, 2},
	{ SDE_IRQ_TYPE_HIST_DSPP_RSTSEQ, DSPP_0,
		SDE_INTR_HIST_DSPP_0_RSTSEQ_DONE, 2},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 2},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 2},
	/* irq_idx: 80-83 */
	{ SDE_IRQ_TYPE_HIST_DSPP_DONE, DSPP_1, SDE_INTR_HIST_DSPP_1_DONE, 2},
	{ SDE_IRQ_TYPE_HIST_DSPP_RSTSEQ, DSPP_1,
		SDE_INTR_HIST_DSPP_1_RSTSEQ_DONE, 2},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 2},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 2},
	/* irq_idx: 84-87 */
	{ SDE_IRQ_TYPE_HIST_DSPP_DONE, DSPP_2, SDE_INTR_HIST_DSPP_2_DONE, 2},
	{ SDE_IRQ_TYPE_HIST_DSPP_RSTSEQ, DSPP_2,
		SDE_INTR_HIST_DSPP_2_RSTSEQ_DONE, 2},
	{ SDE_IRQ_TYPE_HIST_DSPP_DONE, DSPP_3, SDE_INTR_HIST_DSPP_3_DONE, 2},
	{ SDE_IRQ_TYPE_HIST_DSPP_RSTSEQ, DSPP_3,
		SDE_INTR_HIST_DSPP_3_RSTSEQ_DONE, 2},
	/* irq_idx: 88-91 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 2},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 2},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 2},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 2},
	/* irq_idx: 92-95 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 2},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 2},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 2},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 2},

	/* BEGIN MAP_RANGE: 96-127 INTF_0_INTR */
	/* irq_idx: 96-99 */
	{ SDE_IRQ_TYPE_SFI_VIDEO_IN, INTF_0,
		SDE_INTR_VIDEO_INTO_STATIC, 3},
	{ SDE_IRQ_TYPE_SFI_VIDEO_OUT, INTF_0,
		SDE_INTR_VIDEO_OUTOF_STATIC, 3},
	{ SDE_IRQ_TYPE_SFI_CMD_0_IN, INTF_0,
		SDE_INTR_DSICMD_0_INTO_STATIC, 3},
	{ SDE_IRQ_TYPE_SFI_CMD_0_OUT, INTF_0,
		SDE_INTR_DSICMD_0_OUTOF_STATIC, 3},
	/* irq_idx: 100-103 */
	{ SDE_IRQ_TYPE_SFI_CMD_1_IN, INTF_0,
		SDE_INTR_DSICMD_1_INTO_STATIC, 3},
	{ SDE_IRQ_TYPE_SFI_CMD_1_OUT, INTF_0,
		SDE_INTR_DSICMD_1_OUTOF_STATIC, 3},
	{ SDE_IRQ_TYPE_SFI_CMD_2_IN, INTF_0,
		SDE_INTR_DSICMD_2_INTO_STATIC, 3},
	{ SDE_IRQ_TYPE_SFI_CMD_2_OUT, INTF_0,
		SDE_INTR_DSICMD_2_OUTOF_STATIC, 3},
	/* irq_idx: 104-107 */
	{ SDE_IRQ_TYPE_PROG_LINE, INTF_0, SDE_INTR_PROG_LINE, 3},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	/* irq_idx: 108-111 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	/* irq_idx: 112-115 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	/* irq_idx: 116-119 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	/* irq_idx: 120-123 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	/* irq_idx: 124-127 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 3},

	/* BEGIN MAP_RANGE: 128-159 INTF_1_INTR */
	/* irq_idx: 128-131 */
	{ SDE_IRQ_TYPE_SFI_VIDEO_IN, INTF_1,
		SDE_INTR_VIDEO_INTO_STATIC, 4},
	{ SDE_IRQ_TYPE_SFI_VIDEO_OUT, INTF_1,
		SDE_INTR_VIDEO_OUTOF_STATIC, 4},
	{ SDE_IRQ_TYPE_SFI_CMD_0_IN, INTF_1,
		SDE_INTR_DSICMD_0_INTO_STATIC, 4},
	{ SDE_IRQ_TYPE_SFI_CMD_0_OUT, INTF_1,
		SDE_INTR_DSICMD_0_OUTOF_STATIC, 4},
	/* irq_idx: 132-135 */
	{ SDE_IRQ_TYPE_SFI_CMD_1_IN, INTF_1,
		SDE_INTR_DSICMD_1_INTO_STATIC, 4},
	{ SDE_IRQ_TYPE_SFI_CMD_1_OUT, INTF_1,
		SDE_INTR_DSICMD_1_OUTOF_STATIC, 4},
	{ SDE_IRQ_TYPE_SFI_CMD_2_IN, INTF_1,
		SDE_INTR_DSICMD_2_INTO_STATIC, 4},
	{ SDE_IRQ_TYPE_SFI_CMD_2_OUT, INTF_1,
		SDE_INTR_DSICMD_2_OUTOF_STATIC, 4},
	/* irq_idx: 136-139 */
	{ SDE_IRQ_TYPE_PROG_LINE, INTF_1, SDE_INTR_PROG_LINE, 4},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	/* irq_idx: 140-143 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	/* irq_idx: 144-147 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	/* irq_idx: 148-151 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	/* irq_idx: 152-155 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	/* irq_idx: 156-159 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 4},

	/* BEGIN MAP_RANGE: 160-191 INTF_2_INTR */
	/* irq_idx: 160-163 */
	{ SDE_IRQ_TYPE_SFI_VIDEO_IN, INTF_2,
		SDE_INTR_VIDEO_INTO_STATIC, 5},
	{ SDE_IRQ_TYPE_SFI_VIDEO_OUT, INTF_2,
		SDE_INTR_VIDEO_OUTOF_STATIC, 5},
	{ SDE_IRQ_TYPE_SFI_CMD_0_IN, INTF_2,
		SDE_INTR_DSICMD_0_INTO_STATIC, 5},
	{ SDE_IRQ_TYPE_SFI_CMD_0_OUT, INTF_2,
		SDE_INTR_DSICMD_0_OUTOF_STATIC, 5},
	/* irq_idx: 164-167 */
	{ SDE_IRQ_TYPE_SFI_CMD_1_IN, INTF_2,
		SDE_INTR_DSICMD_1_INTO_STATIC, 5},
	{ SDE_IRQ_TYPE_SFI_CMD_1_OUT, INTF_2,
		SDE_INTR_DSICMD_1_OUTOF_STATIC, 5},
	{ SDE_IRQ_TYPE_SFI_CMD_2_IN, INTF_2,
		SDE_INTR_DSICMD_2_INTO_STATIC, 5},
	{ SDE_IRQ_TYPE_SFI_CMD_2_OUT, INTF_2,
		SDE_INTR_DSICMD_2_OUTOF_STATIC, 5},
	/* irq_idx: 168-171 */
	{ SDE_IRQ_TYPE_PROG_LINE, INTF_2, SDE_INTR_PROG_LINE, 5},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	/* irq_idx: 172-175 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	/* irq_idx: 176-179 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	/* irq_idx: 180-183 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	/* irq_idx: 184-187 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	/* irq_idx: 188-191 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 5},

	/* BEGIN MAP_RANGE: 192-223 INTF_3_INTR */
	/* irq_idx: 192-195 */
	{ SDE_IRQ_TYPE_SFI_VIDEO_IN, INTF_3,
		SDE_INTR_VIDEO_INTO_STATIC, 6},
	{ SDE_IRQ_TYPE_SFI_VIDEO_OUT, INTF_3,
		SDE_INTR_VIDEO_OUTOF_STATIC, 6},
	{ SDE_IRQ_TYPE_SFI_CMD_0_IN, INTF_3,
		SDE_INTR_DSICMD_0_INTO_STATIC, 6},
	{ SDE_IRQ_TYPE_SFI_CMD_0_OUT, INTF_3,
		SDE_INTR_DSICMD_0_OUTOF_STATIC, 6},
	/* irq_idx: 196-199 */
	{ SDE_IRQ_TYPE_SFI_CMD_1_IN, INTF_3,
		SDE_INTR_DSICMD_1_INTO_STATIC, 6},
	{ SDE_IRQ_TYPE_SFI_CMD_1_OUT, INTF_3,
		SDE_INTR_DSICMD_1_OUTOF_STATIC, 6},
	{ SDE_IRQ_TYPE_SFI_CMD_2_IN, INTF_3,
		SDE_INTR_DSICMD_2_INTO_STATIC, 6},
	{ SDE_IRQ_TYPE_SFI_CMD_2_OUT, INTF_3,
		SDE_INTR_DSICMD_2_OUTOF_STATIC, 6},
	/* irq_idx: 200-203 */
	{ SDE_IRQ_TYPE_PROG_LINE, INTF_3, SDE_INTR_PROG_LINE, 6},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	/* irq_idx: 204-207 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	/* irq_idx: 208-211 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	/* irq_idx: 212-215 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	/* irq_idx: 216-219 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	/* irq_idx: 220-223 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 6},

	/* BEGIN MAP_RANGE: 224-255 INTF_4_INTR */
	/* irq_idx: 224-227 */
	{ SDE_IRQ_TYPE_SFI_VIDEO_IN, INTF_4,
		SDE_INTR_VIDEO_INTO_STATIC, 7},
	{ SDE_IRQ_TYPE_SFI_VIDEO_OUT, INTF_4,
		SDE_INTR_VIDEO_OUTOF_STATIC, 7},
	{ SDE_IRQ_TYPE_SFI_CMD_0_IN, INTF_4,
		SDE_INTR_DSICMD_0_INTO_STATIC, 7},
	{ SDE_IRQ_TYPE_SFI_CMD_0_OUT, INTF_4,
		SDE_INTR_DSICMD_0_OUTOF_STATIC, 7},
	/* irq_idx: 228-231 */
	{ SDE_IRQ_TYPE_SFI_CMD_1_IN, INTF_4,
		SDE_INTR_DSICMD_1_INTO_STATIC, 7},
	{ SDE_IRQ_TYPE_SFI_CMD_1_OUT, INTF_4,
		SDE_INTR_DSICMD_1_OUTOF_STATIC, 7},
	{ SDE_IRQ_TYPE_SFI_CMD_2_IN, INTF_4,
		SDE_INTR_DSICMD_2_INTO_STATIC, 7},
	{ SDE_IRQ_TYPE_SFI_CMD_2_OUT, INTF_4,
		SDE_INTR_DSICMD_2_OUTOF_STATIC, 7},
	/* irq_idx: 232-235 */
	{ SDE_IRQ_TYPE_PROG_LINE, INTF_4, SDE_INTR_PROG_LINE, 7},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	/* irq_idx: 236-239 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	/* irq_idx: 240-243 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	/* irq_idx: 244-247 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	/* irq_idx: 248-251 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	/* irq_idx: 252-255 */
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
	{ SDE_IRQ_TYPE_RESERVED, 0, 0, 7},
};

static int sde_hw_intr_irqidx_lookup(enum sde_intr_type intr_type,
		u32 instance_idx)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sde_irq_map); i++) {
		if (intr_type == sde_irq_map[i].intr_type &&
			instance_idx == sde_irq_map[i].instance_idx)
			return i;
	}

	pr_debug("IRQ lookup fail!! intr_type=%d, instance_idx=%d\n",
			intr_type, instance_idx);
	return -EINVAL;
}

static void sde_hw_intr_set_mask(struct sde_hw_intr *intr, uint32_t reg_off,
		uint32_t mask)
{
	SDE_REG_WRITE(&intr->hw, reg_off, mask);
}

static void sde_hw_intr_dispatch_irq(struct sde_hw_intr *intr,
		void (*cbfunc)(void *, int),
		void *arg)
{
	int reg_idx;
	int irq_idx;
	int start_idx;
	int end_idx;
	u32 irq_status;
	unsigned long irq_flags;

	/*
	 * The dispatcher will save the IRQ status before calling here.
	 * Now need to go through each IRQ status and find matching
	 * irq lookup index.
	 */
	spin_lock_irqsave(&intr->status_lock, irq_flags);
	for (reg_idx = 0; reg_idx < ARRAY_SIZE(sde_intr_set); reg_idx++) {
		irq_status = intr->save_irq_status[reg_idx];

		/*
		 * Each Interrupt register has a range of 32 indexes, and
		 * that is static for sde_irq_map.
		 */
		start_idx = reg_idx * 32;
		end_idx = start_idx + 32;

		/*
		 * Search through matching intr status from irq map.
		 * start_idx and end_idx defined the search range in
		 * the sde_irq_map.
		 */
		for (irq_idx = start_idx;
				(irq_idx < end_idx) && irq_status;
				irq_idx++)
			if ((irq_status & sde_irq_map[irq_idx].irq_mask) &&
				(sde_irq_map[irq_idx].reg_idx == reg_idx)) {
				/*
				 * Once a match on irq mask, perform a callback
				 * to the given cbfunc. cbfunc will take care
				 * the interrupt status clearing. If cbfunc is
				 * not provided, then the interrupt clearing
				 * is here.
				 */
				if (cbfunc)
					cbfunc(arg, irq_idx);
				else
					intr->ops.clear_interrupt_status(
							intr, irq_idx);

				/*
				 * When callback finish, clear the irq_status
				 * with the matching mask. Once irq_status
				 * is all cleared, the search can be stopped.
				 */
				irq_status &= ~sde_irq_map[irq_idx].irq_mask;
			}
	}
	spin_unlock_irqrestore(&intr->status_lock, irq_flags);
}

static int sde_hw_intr_enable_irq(struct sde_hw_intr *intr, int irq_idx)
{
	int reg_idx;
	unsigned long irq_flags;
	const struct sde_intr_reg *reg;
	const struct sde_irq_type *irq;
	const char *dbgstr = NULL;
	uint32_t cache_irq_mask;

	if (irq_idx < 0 || irq_idx >= ARRAY_SIZE(sde_irq_map)) {
		pr_err("invalid IRQ index: [%d]\n", irq_idx);
		return -EINVAL;
	}

	irq = &sde_irq_map[irq_idx];
	reg_idx = irq->reg_idx;
	reg = &sde_intr_set[reg_idx];

	spin_lock_irqsave(&intr->mask_lock, irq_flags);
	cache_irq_mask = intr->cache_irq_mask[reg_idx];
	if (cache_irq_mask & irq->irq_mask) {
		dbgstr = "SDE IRQ already set:";
	} else {
		dbgstr = "SDE IRQ enabled:";

		cache_irq_mask |= irq->irq_mask;
		/* Cleaning any pending interrupt */
		SDE_REG_WRITE(&intr->hw, reg->clr_off, irq->irq_mask);
		/* Enabling interrupts with the new mask */
		SDE_REG_WRITE(&intr->hw, reg->en_off, cache_irq_mask);

		intr->cache_irq_mask[reg_idx] = cache_irq_mask;
	}
	spin_unlock_irqrestore(&intr->mask_lock, irq_flags);

	pr_debug("%s MASK:0x%.8x, CACHE-MASK:0x%.8x\n", dbgstr,
			irq->irq_mask, cache_irq_mask);

	return 0;
}

static int sde_hw_intr_disable_irq(struct sde_hw_intr *intr, int irq_idx)
{
	int reg_idx;
	unsigned long irq_flags;
	const struct sde_intr_reg *reg;
	const struct sde_irq_type *irq;
	const char *dbgstr = NULL;
	uint32_t cache_irq_mask;

	if (irq_idx < 0 || irq_idx >= ARRAY_SIZE(sde_irq_map)) {
		pr_err("invalid IRQ index: [%d]\n", irq_idx);
		return -EINVAL;
	}

	irq = &sde_irq_map[irq_idx];
	reg_idx = irq->reg_idx;
	reg = &sde_intr_set[reg_idx];

	spin_lock_irqsave(&intr->mask_lock, irq_flags);
	cache_irq_mask = intr->cache_irq_mask[reg_idx];
	if ((cache_irq_mask & irq->irq_mask) == 0) {
		dbgstr = "SDE IRQ is already cleared:";
	} else {
		dbgstr = "SDE IRQ mask disable:";

		cache_irq_mask &= ~irq->irq_mask;
		/* Disable interrupts based on the new mask */
		SDE_REG_WRITE(&intr->hw, reg->en_off, cache_irq_mask);
		/* Cleaning any pending interrupt */
		SDE_REG_WRITE(&intr->hw, reg->clr_off, irq->irq_mask);

		intr->cache_irq_mask[reg_idx] = cache_irq_mask;
	}
	spin_unlock_irqrestore(&intr->mask_lock, irq_flags);

	pr_debug("%s MASK:0x%.8x, CACHE-MASK:0x%.8x\n", dbgstr,
			irq->irq_mask, cache_irq_mask);

	return 0;
}

static int sde_hw_intr_clear_irqs(struct sde_hw_intr *intr)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sde_intr_set); i++)
		SDE_REG_WRITE(&intr->hw, sde_intr_set[i].clr_off, 0xffffffff);

	return 0;
}

static int sde_hw_intr_disable_irqs(struct sde_hw_intr *intr)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sde_intr_set); i++)
		SDE_REG_WRITE(&intr->hw, sde_intr_set[i].en_off, 0x00000000);

	return 0;
}

static int sde_hw_intr_get_valid_interrupts(struct sde_hw_intr *intr,
		uint32_t *mask)
{
	*mask = IRQ_SOURCE_MDP | IRQ_SOURCE_DSI0 | IRQ_SOURCE_DSI1
		| IRQ_SOURCE_HDMI | IRQ_SOURCE_EDP;
	return 0;
}

static int sde_hw_intr_get_interrupt_sources(struct sde_hw_intr *intr,
		uint32_t *sources)
{
	*sources = SDE_REG_READ(&intr->hw, HW_INTR_STATUS);
	return 0;
}

static void sde_hw_intr_get_interrupt_statuses(struct sde_hw_intr *intr)
{
	int i;
	u32 enable_mask;
	unsigned long irq_flags;

	spin_lock_irqsave(&intr->status_lock, irq_flags);
	for (i = 0; i < ARRAY_SIZE(sde_intr_set); i++) {
		/* Read interrupt status */
		intr->save_irq_status[i] = SDE_REG_READ(&intr->hw,
				sde_intr_set[i].status_off);

		/* Read enable mask */
		enable_mask = SDE_REG_READ(&intr->hw, sde_intr_set[i].en_off);

		/* and clear the interrupt */
		if (intr->save_irq_status[i])
			SDE_REG_WRITE(&intr->hw, sde_intr_set[i].clr_off,
					intr->save_irq_status[i]);

		/* Finally update IRQ status based on enable mask */
		intr->save_irq_status[i] &= enable_mask;
	}
	spin_unlock_irqrestore(&intr->status_lock, irq_flags);
}

static void sde_hw_intr_clear_interrupt_status(struct sde_hw_intr *intr,
		int irq_idx)
{
	int reg_idx;
	unsigned long irq_flags;

	spin_lock_irqsave(&intr->mask_lock, irq_flags);

	reg_idx = sde_irq_map[irq_idx].reg_idx;
	SDE_REG_WRITE(&intr->hw, sde_intr_set[reg_idx].clr_off,
			sde_irq_map[irq_idx].irq_mask);

	spin_unlock_irqrestore(&intr->mask_lock, irq_flags);
}

static u32 sde_hw_intr_get_interrupt_status(struct sde_hw_intr *intr,
		int irq_idx, bool clear)
{
	int reg_idx;
	unsigned long irq_flags;
	u32 intr_status;

	spin_lock_irqsave(&intr->mask_lock, irq_flags);

	reg_idx = sde_irq_map[irq_idx].reg_idx;
	intr_status = SDE_REG_READ(&intr->hw,
			sde_intr_set[reg_idx].status_off) &
					sde_irq_map[irq_idx].irq_mask;
	if (intr_status && clear)
		SDE_REG_WRITE(&intr->hw, sde_intr_set[reg_idx].clr_off,
				intr_status);

	spin_unlock_irqrestore(&intr->mask_lock, irq_flags);

	return intr_status;
}

static void __setup_intr_ops(struct sde_hw_intr_ops *ops)
{
	ops->set_mask = sde_hw_intr_set_mask;
	ops->irq_idx_lookup = sde_hw_intr_irqidx_lookup;
	ops->enable_irq = sde_hw_intr_enable_irq;
	ops->disable_irq = sde_hw_intr_disable_irq;
	ops->dispatch_irqs = sde_hw_intr_dispatch_irq;
	ops->clear_all_irqs = sde_hw_intr_clear_irqs;
	ops->disable_all_irqs = sde_hw_intr_disable_irqs;
	ops->get_valid_interrupts = sde_hw_intr_get_valid_interrupts;
	ops->get_interrupt_sources = sde_hw_intr_get_interrupt_sources;
	ops->get_interrupt_statuses = sde_hw_intr_get_interrupt_statuses;
	ops->clear_interrupt_status = sde_hw_intr_clear_interrupt_status;
	ops->get_interrupt_status = sde_hw_intr_get_interrupt_status;
}

static struct sde_mdss_base_cfg *__intr_offset(struct sde_mdss_cfg *m,
		void __iomem *addr, struct sde_hw_blk_reg_map *hw)
{
	if (m->mdp_count == 0)
		return NULL;

	hw->base_off = addr;
	hw->blk_off = m->mdss[0].base;
	hw->hwversion = m->hwversion;
	return &m->mdss[0];
}

struct sde_hw_intr *sde_hw_intr_init(void __iomem *addr,
		struct sde_mdss_cfg *m)
{
	struct sde_hw_intr *intr = kzalloc(sizeof(*intr), GFP_KERNEL);
	struct sde_mdss_base_cfg *cfg;

	if (!intr)
		return ERR_PTR(-ENOMEM);

	cfg = __intr_offset(m, addr, &intr->hw);
	if (!cfg) {
		kfree(intr);
		return ERR_PTR(-EINVAL);
	}
	__setup_intr_ops(&intr->ops);

	intr->irq_idx_tbl_size = ARRAY_SIZE(sde_irq_map);

	intr->cache_irq_mask = kcalloc(ARRAY_SIZE(sde_intr_set), sizeof(u32),
			GFP_KERNEL);
	if (intr->cache_irq_mask == NULL) {
		kfree(intr);
		return ERR_PTR(-ENOMEM);
	}

	intr->save_irq_status = kcalloc(ARRAY_SIZE(sde_intr_set), sizeof(u32),
			GFP_KERNEL);
	if (intr->save_irq_status == NULL) {
		kfree(intr->cache_irq_mask);
		kfree(intr);
		return ERR_PTR(-ENOMEM);
	}

	spin_lock_init(&intr->mask_lock);
	spin_lock_init(&intr->status_lock);

	return intr;
}

void sde_hw_intr_destroy(struct sde_hw_intr *intr)
{
	if (intr) {
		kfree(intr->cache_irq_mask);
		kfree(intr->save_irq_status);
		kfree(intr);
	}
}

