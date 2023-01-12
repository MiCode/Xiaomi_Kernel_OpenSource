// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/bitops.h>
#include <linux/slab.h>

#include "sde_kms.h"
#include "sde_hw_interrupts.h"
#include "sde_hw_util.h"
#include "sde_hw_mdss.h"

/**
 * Register offsets in MDSS register file for the interrupt registers
 * w.r.t. base for that block. Base offsets for IRQs should come from the
 * device tree and get stored in the catalog(irq_offset_list) until they
 * are added to the sde_irq_tbl during the table initialization.
 */
#define HW_INTR_STATUS			0x0010
#define MDP_AD4_INTR_EN_OFF		0x41c
#define MDP_AD4_INTR_CLEAR_OFF		0x424
#define MDP_AD4_INTR_STATUS_OFF		0x420
#define MDP_INTF_TEAR_INTR_EN_OFF	0x0
#define MDP_INTF_TEAR_INTR_STATUS_OFF	0x4
#define MDP_INTF_TEAR_INTR_CLEAR_OFF	0x8
#define MDP_LTM_INTR_EN_OFF		0x50
#define MDP_LTM_INTR_STATUS_OFF		0x54
#define MDP_LTM_INTR_CLEAR_OFF		0x58

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
#define SDE_INTR_PING_PONG_4_DONE BIT(30)
#define SDE_INTR_PING_PONG_5_DONE BIT(31)
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
#define SDE_INTR_PING_PONG_S0_WR_PTR BIT(4)
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
 * Ctl start interrupt status bit definitions
 */
#define SDE_INTR_CTL_0_START BIT(9)
#define SDE_INTR_CTL_1_START BIT(10)
#define SDE_INTR_CTL_2_START BIT(11)
#define SDE_INTR_CTL_3_START BIT(12)
#define SDE_INTR_CTL_4_START BIT(13)
#define SDE_INTR_CTL_5_START BIT(23)

/**
 * Concurrent WB overflow interrupt status bit definitions
 */
#define SDE_INTR_CWB_1_OVERFLOW BIT(8)
#define SDE_INTR_CWB_2_OVERFLOW BIT(14)
#define SDE_INTR_CWB_3_OVERFLOW BIT(15)
#define SDE_INTR_CWB_4_OVERFLOW BIT(20)
#define SDE_INTR_CWB_5_OVERFLOW BIT(21)

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
 * AD4 interrupt status bit definitions
 */
#define SDE_INTR_BRIGHTPR_UPDATED BIT(4)
#define SDE_INTR_DARKENH_UPDATED BIT(3)
#define SDE_INTR_STREN_OUTROI_UPDATED BIT(2)
#define SDE_INTR_STREN_INROI_UPDATED BIT(1)
#define SDE_INTR_BACKLIGHT_UPDATED BIT(0)

/**
 * INTF Tear IRQ register bit definitions
 */
#define SDE_INTR_INTF_TEAR_AUTOREFRESH_DONE BIT(0)
#define SDE_INTR_INTF_TEAR_WR_PTR BIT(1)
#define SDE_INTR_INTF_TEAR_RD_PTR BIT(2)
#define SDE_INTR_INTF_TEAR_TE_DETECTED BIT(3)
#define SDE_INTR_INTF_TEAR_TEAR_DETECTED BIT(4)

/**
 * LTM interrupt status bit definitions
 */
#define SDE_INTR_LTM_STATS_DONE BIT(0)
#define SDE_INTR_LTM_STATS_WB_PB BIT(5)

/**
 * struct sde_intr_reg - array of SDE register sets
 * @clr_off:	offset to CLEAR reg
 * @en_off:	offset to ENABLE reg
 * @status_off:	offset to STATUS reg
 * @map_idx_start   first offset in the sde_irq_map table
 * @map_idx_end    last offset in the sde_irq_map table
 */
struct sde_intr_reg {
	u32 clr_off;
	u32 en_off;
	u32 status_off;
	u32 map_idx_start;
	u32 map_idx_end;
};

/**
 * struct sde_irq_type - maps each irq with i/f
 * @intr_type:		type of interrupt listed in sde_intr_type
 * @instance_idx:	instance index of the associated HW block in SDE
 * @irq_mask:		corresponding bit in the interrupt status reg
 * @reg_idx:		index in the 'sde_irq_tbl' table, to know which
 *			registers offsets to use.
 */
struct sde_irq_type {
	u32 intr_type;
	u32 instance_idx;
	u32 irq_mask;
	int reg_idx;
};

/**
 * IRQ mapping tables - use for lookup an irq_idx in this table that have
 *                     a matching interface type and instance index.
 * Each of these tables are copied to a dynamically allocated
 * table, that will be used to service each of the irqs
 * -1 indicates an uninitialized value which should be set when copying
 * these tables to the sde_irq_map.
 */
static struct sde_irq_type sde_irq_intr_map[] = {

	{ SDE_IRQ_TYPE_WB_ROT_COMP, WB_0, SDE_INTR_WB_0_DONE, -1},
	{ SDE_IRQ_TYPE_WB_ROT_COMP, WB_1, SDE_INTR_WB_1_DONE, -1},
	{ SDE_IRQ_TYPE_WD_TIMER, WD_TIMER_0, SDE_INTR_WD_TIMER_0_DONE, -1},
	{ SDE_IRQ_TYPE_WD_TIMER, WD_TIMER_1, SDE_INTR_WD_TIMER_1_DONE, -1},

	{ SDE_IRQ_TYPE_WB_WFD_COMP, WB_2, SDE_INTR_WB_2_DONE, -1},
	{ SDE_IRQ_TYPE_WD_TIMER, WD_TIMER_2, SDE_INTR_WD_TIMER_2_DONE, -1},
	{ SDE_IRQ_TYPE_WD_TIMER, WD_TIMER_3, SDE_INTR_WD_TIMER_3_DONE, -1},
	{ SDE_IRQ_TYPE_WD_TIMER, WD_TIMER_4, SDE_INTR_WD_TIMER_4_DONE, -1},

	{ SDE_IRQ_TYPE_PING_PONG_COMP, PINGPONG_0,
		SDE_INTR_PING_PONG_0_DONE, -1},
	{ SDE_IRQ_TYPE_PING_PONG_COMP, PINGPONG_1,
		SDE_INTR_PING_PONG_1_DONE, -1},
	{ SDE_IRQ_TYPE_PING_PONG_COMP, PINGPONG_2,
		SDE_INTR_PING_PONG_2_DONE, -1},
	{ SDE_IRQ_TYPE_PING_PONG_COMP, PINGPONG_3,
		SDE_INTR_PING_PONG_3_DONE, -1},

	{ SDE_IRQ_TYPE_PING_PONG_RD_PTR, PINGPONG_0,
		SDE_INTR_PING_PONG_0_RD_PTR, -1},
	{ SDE_IRQ_TYPE_PING_PONG_RD_PTR, PINGPONG_1,
		SDE_INTR_PING_PONG_1_RD_PTR, -1},
	{ SDE_IRQ_TYPE_PING_PONG_RD_PTR, PINGPONG_2,
		SDE_INTR_PING_PONG_2_RD_PTR, -1},
	{ SDE_IRQ_TYPE_PING_PONG_RD_PTR, PINGPONG_3,
		SDE_INTR_PING_PONG_3_RD_PTR, -1},

	{ SDE_IRQ_TYPE_PING_PONG_WR_PTR, PINGPONG_0,
		SDE_INTR_PING_PONG_0_WR_PTR, -1},
	{ SDE_IRQ_TYPE_PING_PONG_WR_PTR, PINGPONG_1,
		SDE_INTR_PING_PONG_1_WR_PTR, -1},
	{ SDE_IRQ_TYPE_PING_PONG_WR_PTR, PINGPONG_2,
		SDE_INTR_PING_PONG_2_WR_PTR, -1},
	{ SDE_IRQ_TYPE_PING_PONG_WR_PTR, PINGPONG_3,
		SDE_INTR_PING_PONG_3_WR_PTR, -1},

	{ SDE_IRQ_TYPE_PING_PONG_AUTO_REF, PINGPONG_0,
		SDE_INTR_PING_PONG_0_AUTOREFRESH_DONE, -1},
	{ SDE_IRQ_TYPE_PING_PONG_AUTO_REF, PINGPONG_1,
		SDE_INTR_PING_PONG_1_AUTOREFRESH_DONE, -1},
	{ SDE_IRQ_TYPE_PING_PONG_AUTO_REF, PINGPONG_2,
		SDE_INTR_PING_PONG_2_AUTOREFRESH_DONE, -1},
	{ SDE_IRQ_TYPE_PING_PONG_AUTO_REF, PINGPONG_3,
		SDE_INTR_PING_PONG_3_AUTOREFRESH_DONE, -1},

	{ SDE_IRQ_TYPE_INTF_UNDER_RUN, INTF_0, SDE_INTR_INTF_0_UNDERRUN, -1},
	{ SDE_IRQ_TYPE_INTF_VSYNC, INTF_0, SDE_INTR_INTF_0_VSYNC, -1},
	{ SDE_IRQ_TYPE_INTF_UNDER_RUN, INTF_1, SDE_INTR_INTF_1_UNDERRUN, -1},
	{ SDE_IRQ_TYPE_INTF_VSYNC, INTF_1, SDE_INTR_INTF_1_VSYNC, -1},

	{ SDE_IRQ_TYPE_INTF_UNDER_RUN, INTF_2, SDE_INTR_INTF_2_UNDERRUN, -1},
	{ SDE_IRQ_TYPE_INTF_VSYNC, INTF_2, SDE_INTR_INTF_2_VSYNC, -1},
	{ SDE_IRQ_TYPE_INTF_UNDER_RUN, INTF_3, SDE_INTR_INTF_3_UNDERRUN, -1},
	{ SDE_IRQ_TYPE_INTF_VSYNC, INTF_3, SDE_INTR_INTF_3_VSYNC, -1},
};

static struct sde_irq_type sde_irq_intr2_map[] = {

	{ SDE_IRQ_TYPE_PING_PONG_AUTO_REF, PINGPONG_S0,
		SDE_INTR_PING_PONG_S0_AUTOREFRESH_DONE, -1},
	{ SDE_IRQ_TYPE_PING_PONG_WR_PTR, PINGPONG_S0,
		SDE_INTR_PING_PONG_S0_WR_PTR, -1},

	{ SDE_IRQ_TYPE_CWB_OVERFLOW, CWB_1, SDE_INTR_CWB_1_OVERFLOW, -1},

	{ SDE_IRQ_TYPE_PING_PONG_RD_PTR, PINGPONG_S0,
		SDE_INTR_PING_PONG_S0_RD_PTR, -1},

	{ SDE_IRQ_TYPE_CTL_START, CTL_0,
		SDE_INTR_CTL_0_START, -1},
	{ SDE_IRQ_TYPE_CTL_START, CTL_1,
		SDE_INTR_CTL_1_START, -1},
	{ SDE_IRQ_TYPE_CTL_START, CTL_2,
		SDE_INTR_CTL_2_START, -1},
	{ SDE_IRQ_TYPE_CTL_START, CTL_3,
		SDE_INTR_CTL_3_START, -1},
	{ SDE_IRQ_TYPE_CTL_START, CTL_4,
		SDE_INTR_CTL_4_START, -1},
	{ SDE_IRQ_TYPE_CTL_START, CTL_5,
		SDE_INTR_CTL_5_START, -1},

	{ SDE_IRQ_TYPE_CWB_OVERFLOW, CWB_2, SDE_INTR_CWB_2_OVERFLOW, -1},
	{ SDE_IRQ_TYPE_CWB_OVERFLOW, CWB_3, SDE_INTR_CWB_3_OVERFLOW, -1},

	{ SDE_IRQ_TYPE_PING_PONG_TEAR_CHECK, PINGPONG_0,
		SDE_INTR_PING_PONG_0_TEAR_DETECTED, -1},
	{ SDE_IRQ_TYPE_PING_PONG_TEAR_CHECK, PINGPONG_1,
		SDE_INTR_PING_PONG_1_TEAR_DETECTED, -1},
	{ SDE_IRQ_TYPE_PING_PONG_TEAR_CHECK, PINGPONG_2,
		SDE_INTR_PING_PONG_2_TEAR_DETECTED, -1},
	{ SDE_IRQ_TYPE_PING_PONG_TEAR_CHECK, PINGPONG_3,
		SDE_INTR_PING_PONG_3_TEAR_DETECTED, -1},

	{ SDE_IRQ_TYPE_CWB_OVERFLOW, CWB_4, SDE_INTR_CWB_4_OVERFLOW, -1},
	{ SDE_IRQ_TYPE_CWB_OVERFLOW, CWB_5, SDE_INTR_CWB_5_OVERFLOW, -1},

	{ SDE_IRQ_TYPE_PING_PONG_TEAR_CHECK, PINGPONG_S0,
		SDE_INTR_PING_PONG_S0_TEAR_DETECTED, -1},

	{ SDE_IRQ_TYPE_PING_PONG_TE_CHECK, PINGPONG_0,
		SDE_INTR_PING_PONG_0_TE_DETECTED, -1},
	{ SDE_IRQ_TYPE_PING_PONG_TE_CHECK, PINGPONG_1,
		SDE_INTR_PING_PONG_1_TE_DETECTED, -1},
	{ SDE_IRQ_TYPE_PING_PONG_TE_CHECK, PINGPONG_2,
		SDE_INTR_PING_PONG_2_TE_DETECTED, -1},
	{ SDE_IRQ_TYPE_PING_PONG_TE_CHECK, PINGPONG_3,
		SDE_INTR_PING_PONG_3_TE_DETECTED, -1},

	{ SDE_IRQ_TYPE_PING_PONG_TE_CHECK, PINGPONG_S0,
		SDE_INTR_PING_PONG_S0_TE_DETECTED, -1},

	{ SDE_IRQ_TYPE_PING_PONG_COMP, PINGPONG_4,
		SDE_INTR_PING_PONG_4_DONE, -1},
	{ SDE_IRQ_TYPE_PING_PONG_COMP, PINGPONG_5,
		SDE_INTR_PING_PONG_5_DONE, -1},
};

static struct sde_irq_type sde_irq_hist_map[] = {
	{ SDE_IRQ_TYPE_HIST_VIG_DONE, SSPP_VIG0, SDE_INTR_HIST_VIG_0_DONE, -1},
	{ SDE_IRQ_TYPE_HIST_VIG_RSTSEQ, SSPP_VIG0,
		SDE_INTR_HIST_VIG_0_RSTSEQ_DONE, -1},
	{ SDE_IRQ_TYPE_HIST_VIG_DONE, SSPP_VIG1, SDE_INTR_HIST_VIG_1_DONE, -1},
	{ SDE_IRQ_TYPE_HIST_VIG_RSTSEQ, SSPP_VIG1,
		SDE_INTR_HIST_VIG_1_RSTSEQ_DONE, -1},
	{ SDE_IRQ_TYPE_HIST_VIG_DONE, SSPP_VIG2, SDE_INTR_HIST_VIG_2_DONE, -1},
	{ SDE_IRQ_TYPE_HIST_VIG_RSTSEQ, SSPP_VIG2,
		SDE_INTR_HIST_VIG_2_RSTSEQ_DONE, -1},
	{ SDE_IRQ_TYPE_HIST_VIG_DONE, SSPP_VIG3, SDE_INTR_HIST_VIG_3_DONE, -1},
	{ SDE_IRQ_TYPE_HIST_VIG_RSTSEQ, SSPP_VIG3,
		SDE_INTR_HIST_VIG_3_RSTSEQ_DONE, -1},

	{ SDE_IRQ_TYPE_HIST_DSPP_DONE, DSPP_0, SDE_INTR_HIST_DSPP_0_DONE, -1},
	{ SDE_IRQ_TYPE_HIST_DSPP_RSTSEQ, DSPP_0,
		SDE_INTR_HIST_DSPP_0_RSTSEQ_DONE, -1},
	{ SDE_IRQ_TYPE_HIST_DSPP_DONE, DSPP_1, SDE_INTR_HIST_DSPP_1_DONE, -1},
	{ SDE_IRQ_TYPE_HIST_DSPP_RSTSEQ, DSPP_1,
		SDE_INTR_HIST_DSPP_1_RSTSEQ_DONE, -1},
	{ SDE_IRQ_TYPE_HIST_DSPP_DONE, DSPP_2, SDE_INTR_HIST_DSPP_2_DONE, -1},
	{ SDE_IRQ_TYPE_HIST_DSPP_RSTSEQ, DSPP_2,
		SDE_INTR_HIST_DSPP_2_RSTSEQ_DONE, -1},
	{ SDE_IRQ_TYPE_HIST_DSPP_DONE, DSPP_3, SDE_INTR_HIST_DSPP_3_DONE, -1},
	{ SDE_IRQ_TYPE_HIST_DSPP_RSTSEQ, DSPP_3,
		SDE_INTR_HIST_DSPP_3_RSTSEQ_DONE, -1},
};

static struct sde_irq_type sde_irq_intf_map[] = {
	{ SDE_IRQ_TYPE_SFI_VIDEO_IN, -1,
		SDE_INTR_VIDEO_INTO_STATIC, -1},
	{ SDE_IRQ_TYPE_SFI_VIDEO_OUT, -1,
		SDE_INTR_VIDEO_OUTOF_STATIC, -1},
	{ SDE_IRQ_TYPE_SFI_CMD_0_IN, -1,
		SDE_INTR_DSICMD_0_INTO_STATIC, -1},
	{ SDE_IRQ_TYPE_SFI_CMD_0_OUT, -1,
		SDE_INTR_DSICMD_0_OUTOF_STATIC, -1},

	{ SDE_IRQ_TYPE_SFI_CMD_1_IN, -1,
		SDE_INTR_DSICMD_1_INTO_STATIC, -1},
	{ SDE_IRQ_TYPE_SFI_CMD_1_OUT, -1,
		SDE_INTR_DSICMD_1_OUTOF_STATIC, -1},
	{ SDE_IRQ_TYPE_SFI_CMD_2_IN, -1,
		SDE_INTR_DSICMD_2_INTO_STATIC, -1},
	{ SDE_IRQ_TYPE_SFI_CMD_2_OUT, -1,
		SDE_INTR_DSICMD_2_OUTOF_STATIC, -1},

	{ SDE_IRQ_TYPE_PROG_LINE, -1, SDE_INTR_PROG_LINE, -1},
};

static struct sde_irq_type sde_irq_ad4_map[] = {
	{ SDE_IRQ_TYPE_AD4_BL_DONE, -1, SDE_INTR_BACKLIGHT_UPDATED, -1},
};

static struct sde_irq_type sde_irq_intf_te_map[] = {
	{ SDE_IRQ_TYPE_INTF_TEAR_AUTO_REF, -1,
		SDE_INTR_INTF_TEAR_AUTOREFRESH_DONE, -1},
	{ SDE_IRQ_TYPE_INTF_TEAR_WR_PTR, -1,
		SDE_INTR_INTF_TEAR_WR_PTR, -1},
	{ SDE_IRQ_TYPE_INTF_TEAR_RD_PTR, -1,
		SDE_INTR_INTF_TEAR_RD_PTR, -1},
	{ SDE_IRQ_TYPE_INTF_TEAR_TEAR_CHECK, -1,
		SDE_INTR_INTF_TEAR_TEAR_DETECTED, -1},
};

static struct sde_irq_type sde_irq_ltm_map[] = {
	{ SDE_IRQ_TYPE_LTM_STATS_DONE, -1, SDE_INTR_LTM_STATS_DONE, -1},
	{ SDE_IRQ_TYPE_LTM_STATS_WB_PB, -1, SDE_INTR_LTM_STATS_WB_PB, -1},
};

static int sde_hw_intr_irqidx_lookup(struct sde_hw_intr *intr,
	enum sde_intr_type intr_type, u32 instance_idx)
{
	int i;

	for (i = 0; i < intr->sde_irq_map_size; i++) {
		if (intr_type == intr->sde_irq_map[i].intr_type &&
			instance_idx == intr->sde_irq_map[i].instance_idx)
			return i;
	}

	pr_debug("IRQ lookup fail!! intr_type=%d, instance_idx=%d\n",
			intr_type, instance_idx);
	return -EINVAL;
}

static void sde_hw_intr_set_mask(struct sde_hw_intr *intr, uint32_t reg_off,
		uint32_t mask)
{
	if (!intr)
		return;

	SDE_REG_WRITE(&intr->hw, reg_off, mask);

	/* ensure register writes go through */
	wmb();
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

	if (!intr)
		return;

	/*
	 * The dispatcher will save the IRQ status before calling here.
	 * Now need to go through each IRQ status and find matching
	 * irq lookup index.
	 */
	spin_lock_irqsave(&intr->irq_lock, irq_flags);
	for (reg_idx = 0; reg_idx < intr->sde_irq_size; reg_idx++) {
		irq_status = intr->save_irq_status[reg_idx];

		/*
		 * Each Interrupt register has dynamic range of indexes,
		 * initialized during hw_intr_init when sde_irq_tbl is created.
		 */
		start_idx = intr->sde_irq_tbl[reg_idx].map_idx_start;
		end_idx = intr->sde_irq_tbl[reg_idx].map_idx_end;

		if (start_idx >= intr->sde_irq_map_size ||
				end_idx > intr->sde_irq_map_size)
			continue;

		/*
		 * Search through matching intr status from irq map.
		 * start_idx and end_idx defined the search range in
		 * the sde_irq_map.
		 */
		for (irq_idx = start_idx;
				(irq_idx < end_idx) && irq_status;
				irq_idx++)
			if ((irq_status &
				intr->sde_irq_map[irq_idx].irq_mask) &&
				(intr->sde_irq_map[irq_idx].reg_idx ==
				 reg_idx)) {
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
					intr->ops.clear_intr_status_nolock(
							intr, irq_idx);

				/*
				 * When callback finish, clear the irq_status
				 * with the matching mask. Once irq_status
				 * is all cleared, the search can be stopped.
				 */
				irq_status &=
					~intr->sde_irq_map[irq_idx].irq_mask;
			}
	}
	spin_unlock_irqrestore(&intr->irq_lock, irq_flags);
}

static int sde_hw_intr_enable_irq_nolock(struct sde_hw_intr *intr, int irq_idx)
{
	int reg_idx;
	const struct sde_intr_reg *reg;
	const struct sde_irq_type *irq;
	const char *dbgstr = NULL;
	uint32_t cache_irq_mask;

	if (!intr)
		return -EINVAL;

	if (irq_idx < 0 || irq_idx >= intr->sde_irq_map_size) {
		pr_err("invalid IRQ index: [%d]\n", irq_idx);
		return -EINVAL;
	}

	irq = &intr->sde_irq_map[irq_idx];
	reg_idx = irq->reg_idx;
	if (reg_idx < 0 || reg_idx > intr->sde_irq_size) {
		pr_err("invalid irq reg:%d irq:%d\n", reg_idx, irq_idx);
		return -EINVAL;
	}

	reg = &intr->sde_irq_tbl[reg_idx];

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

		/* ensure register write goes through */
		wmb();

		intr->cache_irq_mask[reg_idx] = cache_irq_mask;
	}

	pr_debug("%s MASK:0x%.8x, CACHE-MASK:0x%.8x\n", dbgstr,
			irq->irq_mask, cache_irq_mask);

	return 0;
}

static int sde_hw_intr_disable_irq_nolock(struct sde_hw_intr *intr, int irq_idx)
{
	int reg_idx;
	const struct sde_intr_reg *reg;
	const struct sde_irq_type *irq;
	const char *dbgstr = NULL;
	uint32_t cache_irq_mask;

	if (!intr)
		return -EINVAL;

	if (irq_idx < 0 || irq_idx >= intr->sde_irq_map_size) {
		pr_err("invalid IRQ index: [%d]\n", irq_idx);
		return -EINVAL;
	}

	irq = &intr->sde_irq_map[irq_idx];
	reg_idx = irq->reg_idx;
	if (reg_idx < 0 || reg_idx > intr->sde_irq_size) {
		pr_err("invalid irq reg:%d irq:%d\n", reg_idx, irq_idx);
		return -EINVAL;
	}

	reg = &intr->sde_irq_tbl[reg_idx];

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

		/* ensure register write goes through */
		wmb();

		intr->cache_irq_mask[reg_idx] = cache_irq_mask;
	}

	pr_debug("%s MASK:0x%.8x, CACHE-MASK:0x%.8x\n", dbgstr,
			irq->irq_mask, cache_irq_mask);

	return 0;
}

static int sde_hw_intr_clear_irqs(struct sde_hw_intr *intr)
{
	int i;

	if (!intr)
		return -EINVAL;

	for (i = 0; i < intr->sde_irq_size; i++)
		SDE_REG_WRITE(&intr->hw, intr->sde_irq_tbl[i].clr_off,
				0xffffffff);

	/* ensure register writes go through */
	wmb();

	return 0;
}

static int sde_hw_intr_disable_irqs(struct sde_hw_intr *intr)
{
	int i;

	if (!intr)
		return -EINVAL;

	for (i = 0; i < intr->sde_irq_size; i++)
		SDE_REG_WRITE(&intr->hw, intr->sde_irq_tbl[i].en_off,
				0x00000000);

	/* ensure register writes go through */
	wmb();

	return 0;
}

static int sde_hw_intr_get_valid_interrupts(struct sde_hw_intr *intr,
		uint32_t *mask)
{
	if (!intr || !mask)
		return -EINVAL;

	*mask = IRQ_SOURCE_MDP | IRQ_SOURCE_DSI0 | IRQ_SOURCE_DSI1
		| IRQ_SOURCE_HDMI | IRQ_SOURCE_EDP;

	return 0;
}

static int sde_hw_intr_get_interrupt_sources(struct sde_hw_intr *intr,
		uint32_t *sources)
{
	if (!intr || !sources)
		return -EINVAL;

	*sources = SDE_REG_READ(&intr->hw, HW_INTR_STATUS);

	return 0;
}

static void sde_hw_intr_get_interrupt_statuses(struct sde_hw_intr *intr)
{
	int i;
	u32 enable_mask;
	unsigned long irq_flags;

	if (!intr)
		return;

	spin_lock_irqsave(&intr->irq_lock, irq_flags);
	for (i = 0; i < intr->sde_irq_size; i++) {
		/* Read interrupt status */
		intr->save_irq_status[i] = SDE_REG_READ(&intr->hw,
				intr->sde_irq_tbl[i].status_off);

		/* Read enable mask */
		enable_mask = SDE_REG_READ(&intr->hw,
				intr->sde_irq_tbl[i].en_off);

		/* and clear the interrupt */
		if (intr->save_irq_status[i])
			SDE_REG_WRITE(&intr->hw, intr->sde_irq_tbl[i].clr_off,
					intr->save_irq_status[i]);

		/* Finally update IRQ status based on enable mask */
		intr->save_irq_status[i] &= enable_mask;
	}

	/* ensure register writes go through */
	wmb();

	spin_unlock_irqrestore(&intr->irq_lock, irq_flags);
}

static void sde_hw_intr_clear_intr_status_force_mask(struct sde_hw_intr *intr,
						 int irq_idx, u32 irq_mask)
{
	int reg_idx;

	if (!intr)
		return;

	if (irq_idx >= intr->sde_irq_map_size || irq_idx < 0) {
		pr_err("invalid IRQ index: [%d]\n", irq_idx);
		return;
	}

	reg_idx = intr->sde_irq_map[irq_idx].reg_idx;
	if (reg_idx < 0 || reg_idx > intr->sde_irq_size) {
		pr_err("invalid irq reg:%d irq:%d\n", reg_idx, irq_idx);
		return;
	}

	SDE_REG_WRITE(&intr->hw, intr->sde_irq_tbl[reg_idx].clr_off,
			irq_mask);

	/* ensure register writes go through */
	wmb();
}

static void sde_hw_intr_clear_intr_status_nolock(struct sde_hw_intr *intr,
		int irq_idx)
{
	int reg_idx;

	if (!intr)
		return;

	if (irq_idx >= intr->sde_irq_map_size || irq_idx < 0) {
		pr_err("invalid IRQ index: [%d]\n", irq_idx);
		return;
	}

	reg_idx = intr->sde_irq_map[irq_idx].reg_idx;
	if (reg_idx < 0 || reg_idx > intr->sde_irq_size) {
		pr_err("invalid irq reg:%d irq:%d\n", reg_idx, irq_idx);
		return;
	}

	SDE_REG_WRITE(&intr->hw, intr->sde_irq_tbl[reg_idx].clr_off,
			intr->sde_irq_map[irq_idx].irq_mask);

	/* ensure register writes go through */
	wmb();
}

static void sde_hw_intr_clear_interrupt_status(struct sde_hw_intr *intr,
		int irq_idx)
{
	unsigned long irq_flags;

	if (!intr)
		return;

	spin_lock_irqsave(&intr->irq_lock, irq_flags);
	sde_hw_intr_clear_intr_status_nolock(intr, irq_idx);
	spin_unlock_irqrestore(&intr->irq_lock, irq_flags);
}

static u32 sde_hw_intr_get_intr_status_nolock(struct sde_hw_intr *intr,
		int irq_idx, bool clear)
{
	int reg_idx;
	u32 intr_status;

	if (!intr)
		return 0;

	if (irq_idx >= intr->sde_irq_map_size || irq_idx < 0) {
		pr_err("invalid IRQ index: [%d]\n", irq_idx);
		return 0;
	}

	reg_idx = intr->sde_irq_map[irq_idx].reg_idx;
	if (reg_idx < 0 || reg_idx > intr->sde_irq_size) {
		pr_err("invalid irq reg:%d irq:%d\n", reg_idx, irq_idx);
		return 0;
	}

	intr_status = SDE_REG_READ(&intr->hw,
			intr->sde_irq_tbl[reg_idx].status_off) &
					intr->sde_irq_map[irq_idx].irq_mask;
	if (intr_status && clear)
		SDE_REG_WRITE(&intr->hw, intr->sde_irq_tbl[reg_idx].clr_off,
				intr_status);

	/* ensure register writes go through */
	wmb();

	return intr_status;
}

static u32 sde_hw_intr_get_interrupt_status(struct sde_hw_intr *intr,
		int irq_idx, bool clear)
{
	int reg_idx;
	unsigned long irq_flags;
	u32 intr_status;

	if (!intr)
		return 0;

	if (irq_idx >= intr->sde_irq_map_size || irq_idx < 0) {
		pr_err("invalid IRQ index: [%d]\n", irq_idx);
		return 0;
	}

	reg_idx = intr->sde_irq_map[irq_idx].reg_idx;
	if (reg_idx < 0 || reg_idx > intr->sde_irq_size) {
		pr_err("invalid irq reg:%d irq:%d\n", reg_idx, irq_idx);
		return 0;
	}

	spin_lock_irqsave(&intr->irq_lock, irq_flags);

	intr_status = SDE_REG_READ(&intr->hw,
			intr->sde_irq_tbl[reg_idx].status_off) &
					intr->sde_irq_map[irq_idx].irq_mask;
	if (intr_status && clear)
		SDE_REG_WRITE(&intr->hw, intr->sde_irq_tbl[reg_idx].clr_off,
				intr_status);

	/* ensure register writes go through */
	wmb();

	spin_unlock_irqrestore(&intr->irq_lock, irq_flags);

	return intr_status;
}

static u32 sde_hw_intr_get_intr_status_nomask(struct sde_hw_intr *intr,
		int irq_idx, bool clear)
{
	int reg_idx;
	unsigned long irq_flags;
	u32 intr_status = 0;

	if (!intr)
		return 0;

	if (irq_idx >= intr->sde_irq_map_size || irq_idx < 0) {
		pr_err("invalid IRQ index: [%d]\n", irq_idx);
		return 0;
	}

	reg_idx = intr->sde_irq_map[irq_idx].reg_idx;
	if (reg_idx < 0 || reg_idx > intr->sde_irq_size) {
		pr_err("invalid irq reg:%d irq:%d\n", reg_idx, irq_idx);
		return 0;
	}

	spin_lock_irqsave(&intr->irq_lock, irq_flags);
	intr_status = SDE_REG_READ(&intr->hw,
			intr->sde_irq_tbl[reg_idx].status_off);
	spin_unlock_irqrestore(&intr->irq_lock, irq_flags);

	return intr_status;
}

static int _set_sde_irq_tbl_offset_top(struct sde_intr_reg *sde_irq,
		struct sde_intr_irq_offsets *item)
{
	u32 base_offset;

	if (!sde_irq || !item)
		return -EINVAL;

	base_offset = item->base_offset;
	switch (item->instance_idx) {
	case SDE_INTR_TOP_INTR:
		sde_irq->clr_off = base_offset + INTR_CLEAR;
		sde_irq->en_off = base_offset + INTR_EN;
		sde_irq->status_off = base_offset + INTR_STATUS;
		break;
	case SDE_INTR_TOP_INTR2:
		sde_irq->clr_off = base_offset + INTR2_CLEAR;
		sde_irq->en_off = base_offset + INTR2_EN;
		sde_irq->status_off = base_offset + INTR2_STATUS;
		break;
	case SDE_INTR_TOP_HIST_INTR:
		sde_irq->clr_off = base_offset + HIST_INTR_CLEAR;
		sde_irq->en_off = base_offset + HIST_INTR_EN;
		sde_irq->status_off = base_offset + HIST_INTR_STATUS;
		break;
	default:
		pr_err("invalid TOP intr for instance %d\n",
				item->instance_idx);
		return -EINVAL;
	}

	return 0;
}

static int _set_sde_irq_tbl_offset(struct sde_intr_reg *sde_irq,
		struct sde_intr_irq_offsets *item)
{
	u32 base_offset, rc = 0;

	if (!sde_irq || !item)
		return -EINVAL;

	base_offset = item->base_offset;
	switch (item->type) {
	case SDE_INTR_HWBLK_TOP:
		rc = _set_sde_irq_tbl_offset_top(sde_irq, item);
		break;
	case SDE_INTR_HWBLK_INTF:
		sde_irq->clr_off = base_offset + INTF_INTR_CLEAR;
		sde_irq->en_off = base_offset + INTF_INTR_EN;
		sde_irq->status_off = base_offset + INTF_INTR_STATUS;
		break;
	case SDE_INTR_HWBLK_AD4:
		sde_irq->clr_off = base_offset + MDP_AD4_INTR_CLEAR_OFF;
		sde_irq->en_off = base_offset + MDP_AD4_INTR_EN_OFF;
		sde_irq->status_off = base_offset + MDP_AD4_INTR_STATUS_OFF;
		break;
	case SDE_INTR_HWBLK_INTF_TEAR:
		sde_irq->clr_off = base_offset + MDP_INTF_TEAR_INTR_CLEAR_OFF;
		sde_irq->en_off = base_offset + MDP_INTF_TEAR_INTR_EN_OFF;
		sde_irq->status_off = base_offset +
				MDP_INTF_TEAR_INTR_STATUS_OFF;
		break;
	case SDE_INTR_HWBLK_LTM:
		sde_irq->clr_off = base_offset + MDP_LTM_INTR_CLEAR_OFF;
		sde_irq->en_off = base_offset + MDP_LTM_INTR_EN_OFF;
		sde_irq->status_off = base_offset + MDP_LTM_INTR_STATUS_OFF;
		break;
	default:
		pr_err("unrecognized intr blk type %d\n",
				item->type);
		rc = -EINVAL;
	}

	return rc;
}

static void __setup_intr_ops(struct sde_hw_intr_ops *ops)
{
	ops->set_mask = sde_hw_intr_set_mask;
	ops->irq_idx_lookup = sde_hw_intr_irqidx_lookup;
	ops->enable_irq_nolock = sde_hw_intr_enable_irq_nolock;
	ops->disable_irq_nolock = sde_hw_intr_disable_irq_nolock;
	ops->dispatch_irqs = sde_hw_intr_dispatch_irq;
	ops->clear_all_irqs = sde_hw_intr_clear_irqs;
	ops->disable_all_irqs = sde_hw_intr_disable_irqs;
	ops->get_valid_interrupts = sde_hw_intr_get_valid_interrupts;
	ops->get_interrupt_sources = sde_hw_intr_get_interrupt_sources;
	ops->get_interrupt_statuses = sde_hw_intr_get_interrupt_statuses;
	ops->clear_interrupt_status = sde_hw_intr_clear_interrupt_status;
	ops->clear_intr_status_nolock = sde_hw_intr_clear_intr_status_nolock;
	ops->clear_intr_status_force_mask =
				sde_hw_intr_clear_intr_status_force_mask;
	ops->get_interrupt_status = sde_hw_intr_get_interrupt_status;
	ops->get_intr_status_nolock = sde_hw_intr_get_intr_status_nolock;
	ops->get_intr_status_nomask = sde_hw_intr_get_intr_status_nomask;
}

static struct sde_mdss_base_cfg *__intr_offset(struct sde_mdss_cfg *m,
		void __iomem *addr, struct sde_hw_blk_reg_map *hw)
{
	if (!m || !addr || !hw || m->mdp_count == 0)
		return NULL;

	hw->base_off = addr;
	hw->blk_off = m->mdss[0].base;
	hw->hwversion = m->hwversion;
	return &m->mdss[0];
}

void sde_hw_intr_destroy(struct sde_hw_intr *intr)
{
	if (intr) {
		kfree(intr->sde_irq_tbl);
		kfree(intr->sde_irq_map);
		kfree(intr->cache_irq_mask);
		kfree(intr->save_irq_status);
		kfree(intr);
	}
}

static inline u32 _get_irq_map_size_top(enum sde_intr_top_intr inst)
{
	u32 ret = 0;

	switch (inst) {
	case SDE_INTR_TOP_INTR:
		ret = ARRAY_SIZE(sde_irq_intr_map);
		break;
	case SDE_INTR_TOP_INTR2:
		ret = ARRAY_SIZE(sde_irq_intr2_map);
		break;
	case SDE_INTR_TOP_HIST_INTR:
		ret = ARRAY_SIZE(sde_irq_hist_map);
		break;
	default:
		pr_err("invalid top inst:%d\n", inst);
	}

	return ret;
}

static inline u32 _get_irq_map_size(struct sde_intr_irq_offsets *item)
{
	u32 ret = 0;

	switch (item->type) {
	case SDE_INTR_HWBLK_TOP:
		ret = _get_irq_map_size_top(item->instance_idx);
		break;
	case SDE_INTR_HWBLK_INTF:
		ret = ARRAY_SIZE(sde_irq_intf_map);
		break;
	case SDE_INTR_HWBLK_AD4:
		ret = ARRAY_SIZE(sde_irq_ad4_map);
		break;
	case SDE_INTR_HWBLK_INTF_TEAR:
		ret = ARRAY_SIZE(sde_irq_intf_te_map);
		break;
	case SDE_INTR_HWBLK_LTM:
		ret = ARRAY_SIZE(sde_irq_ltm_map);
		break;
	default:
		pr_err("invalid type: %d\n", item->type);
	}

	return ret;
}

static inline struct sde_irq_type *_get_irq_map_addr_top(
		enum sde_intr_top_intr inst)
{
	struct sde_irq_type *ret = NULL;

	switch (inst) {
	case SDE_INTR_TOP_INTR:
		ret = sde_irq_intr_map;
		break;
	case SDE_INTR_TOP_INTR2:
		ret = sde_irq_intr2_map;
		break;
	case SDE_INTR_TOP_HIST_INTR:
		ret = sde_irq_hist_map;
		break;
	default:
		pr_err("invalid top inst:%d\n", inst);
	}

	return ret;
}

static inline struct sde_irq_type *_get_irq_map_addr(
		struct sde_intr_irq_offsets *item)
{
	struct sde_irq_type *ret = NULL;

	switch (item->type) {
	case SDE_INTR_HWBLK_TOP:
		ret = _get_irq_map_addr_top(item->instance_idx);
		break;
	case SDE_INTR_HWBLK_INTF:
		ret = sde_irq_intf_map;
		break;
	case SDE_INTR_HWBLK_AD4:
		ret = sde_irq_ad4_map;
		break;
	case SDE_INTR_HWBLK_INTF_TEAR:
		ret = sde_irq_intf_te_map;
		break;
	case SDE_INTR_HWBLK_LTM:
		ret = sde_irq_ltm_map;
		break;
	default:
		pr_err("invalid type: %d\n", item->type);
	}

	return ret;
}

static int _sde_copy_regs(struct sde_irq_type *sde_irq_map, u32 size,
	struct sde_intr_irq_offsets *item, u32 low_idx, u32 high_idx)
{
	int i, j = 0;
	struct sde_irq_type *src = _get_irq_map_addr(item);
	u32 src_size = _get_irq_map_size(item);

	if (!src)
		return -EINVAL;

	if (low_idx >= size || high_idx > size ||
		(high_idx - low_idx > src_size)) {
		pr_err("invalid size l:%d h:%d dst:%d src:%d\n",
			low_idx, high_idx, size, src_size);
		return -EINVAL;
	}

	for (i = low_idx; i < high_idx; i++)
		sde_irq_map[i] = src[j++];

	return 0;
}

static int _sde_hw_intr_init_irq_tables(struct sde_hw_intr *intr,
	struct sde_mdss_cfg *m)
{
	struct sde_intr_irq_offsets *item;
	int i, sde_irq_tbl_idx = 0, ret = 0;
	u32 low_idx, high_idx;
	u32 sde_irq_map_idx = 0;

	/* Initialize offsets in the sde_irq_map & sde_irq_tbl tables */
	list_for_each_entry(item, &m->irq_offset_list, list) {
		low_idx = sde_irq_map_idx;
		high_idx = low_idx + _get_irq_map_size(item);

		if (sde_irq_tbl_idx >= intr->sde_irq_size ||
			sde_irq_tbl_idx < 0) {
			ret = -EINVAL;
			goto exit;
		}

		/* init sde_irq_map with the global irq mapping table */
		if (_sde_copy_regs(intr->sde_irq_map, intr->sde_irq_map_size,
				item, low_idx, high_idx)) {
			ret = -EINVAL;
			goto exit;
		}

		/* init irq map with its reg & instance idxs in the irq tbl */
		for (i = low_idx; i < high_idx; i++) {
			intr->sde_irq_map[i].reg_idx = sde_irq_tbl_idx;
			if (item->type != SDE_INTR_HWBLK_TOP)
				intr->sde_irq_map[i].instance_idx =
						item->instance_idx;
			pr_debug("sde_irq_map[%d].reg_idx=%d .inst_idx = %d\n",
				i, sde_irq_tbl_idx, item->instance_idx);
		}

		/* track the idx of the mapping table for this irq in
		 * sde_irq_map, this to only access the indexes of this
		 * irq during the irq dispatch
		 */
		intr->sde_irq_tbl[sde_irq_tbl_idx].map_idx_start = low_idx;
		intr->sde_irq_tbl[sde_irq_tbl_idx].map_idx_end = high_idx;
		ret = _set_sde_irq_tbl_offset(
				&intr->sde_irq_tbl[sde_irq_tbl_idx], item);
		if (ret)
			goto exit;

		/* increment idx for both tables accordingly */
		sde_irq_tbl_idx++;
		sde_irq_map_idx = high_idx;
	}

exit:
	sde_hw_catalog_irq_offset_list_delete(&m->irq_offset_list);
	return ret;
}

struct sde_hw_intr *sde_hw_intr_init(void __iomem *addr,
		struct sde_mdss_cfg *m)
{
	struct sde_hw_intr *intr = NULL;
	struct sde_mdss_base_cfg *cfg;
	struct sde_intr_irq_offsets *item;
	u32 irq_regs_count = 0;
	u32 irq_map_count = 0;
	u32 size;
	int ret = 0;

	if (!addr || !m) {
		ret = -EINVAL;
		goto exit;
	}

	intr = kzalloc(sizeof(*intr), GFP_KERNEL);
	if (!intr) {
		ret = -ENOMEM;
		goto exit;
	}

	cfg = __intr_offset(m, addr, &intr->hw);
	if (!cfg) {
		ret = -EINVAL;
		goto exit;
	}
	__setup_intr_ops(&intr->ops);

	/* check how many irq's this target supports */
	list_for_each_entry(item, &m->irq_offset_list, list) {
		size = _get_irq_map_size(item);
		if (!size || irq_map_count >= UINT_MAX - size) {
			pr_err("wrong map cnt idx:%d blk:%d/%d sz:%d cnt:%d\n",
				irq_regs_count, item->type, item->instance_idx,
				size, irq_map_count);
			ret = -EINVAL;
			goto exit;
		}

		irq_regs_count++;
		irq_map_count += size;
	}

	if (irq_regs_count == 0 || irq_map_count == 0) {
		pr_err("invalid irq map: %d %d\n",
				irq_regs_count, irq_map_count);
		ret = -EINVAL;
		goto exit;
	}

	/* Allocate table for the irq registers */
	intr->sde_irq_size = irq_regs_count;
	intr->sde_irq_tbl = kcalloc(irq_regs_count, sizeof(*intr->sde_irq_tbl),
		GFP_KERNEL);
	if (intr->sde_irq_tbl == NULL) {
		ret = -ENOMEM;
		goto exit;
	}

	/* Allocate table with the valid interrupts bits */
	intr->sde_irq_map_size = irq_map_count;
	intr->sde_irq_map = kcalloc(irq_map_count, sizeof(*intr->sde_irq_map),
		GFP_KERNEL);
	if (intr->sde_irq_map == NULL) {
		ret = -ENOMEM;
		goto exit;
	}

	/* Initialize IRQs tables */
	ret = _sde_hw_intr_init_irq_tables(intr, m);
	if (ret)
		goto exit;

	intr->cache_irq_mask = kcalloc(intr->sde_irq_size,
			sizeof(*intr->cache_irq_mask), GFP_KERNEL);
	if (intr->cache_irq_mask == NULL) {
		ret = -ENOMEM;
		goto exit;
	}

	intr->save_irq_status = kcalloc(intr->sde_irq_size,
			sizeof(*intr->save_irq_status), GFP_KERNEL);
	if (intr->save_irq_status == NULL) {
		ret = -ENOMEM;
		goto exit;
	}

	spin_lock_init(&intr->irq_lock);

exit:
	if (ret) {
		sde_hw_intr_destroy(intr);
		return ERR_PTR(ret);
	}

	return intr;
}

