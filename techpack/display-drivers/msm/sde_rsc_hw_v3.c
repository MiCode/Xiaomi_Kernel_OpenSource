// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[sde_rsc_hw:%s:%d]: " fmt, __func__, __LINE__

#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/delay.h>

#include "sde_rsc_priv.h"
#include "sde_rsc_hw.h"
#include "sde_dbg.h"

static int _rsc_hw_qtimer_init(struct sde_rsc_priv *rsc)
{
	pr_debug("rsc hardware qtimer init\n");

	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_QTMR_AC_HW_FRAME_SEL_1,
						0xffffffff, rsc->debug_mode);
	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_QTMR_AC_HW_FRAME_SEL_2,
						0xffffffff, rsc->debug_mode);

	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_QTMR_AC_CNTACR0_FG0,
						0x1, rsc->debug_mode);
	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_QTMR_AC_CNTACR1_FG0,
						0x1, rsc->debug_mode);

	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F0_QTMR_V1_CNTP_CVAL_LO,
						0xffffffff, rsc->debug_mode);
	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F0_QTMR_V1_CNTP_CVAL_HI,
						0xffffffff, rsc->debug_mode);
	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F1_QTMR_V1_CNTP_CVAL_LO,
						0xffffffff, rsc->debug_mode);
	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F1_QTMR_V1_CNTP_CVAL_HI,
						0xffffffff, rsc->debug_mode);

	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F0_QTMR_V1_CNTP_CTL,
						0x1, rsc->debug_mode);
	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F1_QTMR_V1_CNTP_CTL,
						0x1, rsc->debug_mode);

	return 0;
}

static int _rsc_hw_pdc_init(struct sde_rsc_priv *rsc)
{
	pr_debug("rsc hardware pdc init\n");

	dss_reg_w(&rsc->drv_io, SDE_RSCC_PDC_SEQ_START_ADDR_REG_OFFSET_DRV0,
						0x4520, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_PDC_MATCH_VALUE_LO_REG_OFFSET_DRV0,
						0x4510, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_PDC_MATCH_VALUE_HI_REG_OFFSET_DRV0,
						0x4514, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_PDC_SLAVE_ID_DRV0,
						0x1, rsc->debug_mode);

	return 0;
}

static int _rsc_hw_wrapper_init(struct sde_rsc_priv *rsc)
{
	pr_debug("rsc hardware wrapper init\n");

	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_STATIC_WAKEUP_0,
		rsc->timer_config.static_wakeup_time_ns, rsc->debug_mode);

	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_RSCC_MODE_THRESHOLD,
		rsc->timer_config.rsc_mode_threshold_time_ns, rsc->debug_mode);

	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL,
						BIT(8), rsc->debug_mode);

	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_MODE_MIN_THRESHOLD,
		rsc->timer_config.min_threshold_time_ns, rsc->debug_mode);

	return 0;
}

static int _rsc_hw_seq_memory_init_v3(struct sde_rsc_priv *rsc)
{
	const u32 mode_0_start_addr = 0x0;
	const u32 mode_1_start_addr = 0xc;
	const u32 mode_2_start_addr = 0x18;
	u32 br_offset = 0;

	pr_debug("rsc sequencer memory init v2\n");

	/* Mode - 0 sequence */
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x0,
						0xff399ebe, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x4,
						0x20209ebe, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x8,
						0x20202020, rsc->debug_mode);

	/* Mode - 1 sequence */
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0xc,
						0xe0389ebe, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x10,
						0x9ebeff39, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x14,
						0x20202020, rsc->debug_mode);

	/* Mode - 2 sequence */
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x18,
						0xf9b9baa0, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x1c,
						0x999afebd, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x20,
						0x81e1a138, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x24,
						0xe2a2e0ac, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x28,
						0xfd9d3982, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x2c,
						0x2020208c, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x30,
						0x20202020, rsc->debug_mode);

	/* tcs sleep & wake sequence */
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x34,
						0x01a6fcbc, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x38,
						0x20209ce6, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x3c,
						0x01a7fcbc, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x40,
						0x00209ce7, rsc->debug_mode);

	/* branch address */
	if (rsc->hw_drv_ver >= SDE_RSC_HW_MAJOR_MINOR_STEP(2, 0, 5) ||
		rsc->hw_drv_ver == SDE_RSC_HW_MAJOR_MINOR_STEP(1, 9, 0))
		br_offset = 0xf0;

	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_CFG_BR_ADDR_0_DRV0 + br_offset,
						0x34, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_CFG_BR_ADDR_1_DRV0 + br_offset,
						0x3c, rsc->debug_mode);

	/* start address */
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_OVERRIDE_CTRL_DRV0,
					mode_0_start_addr,
					rsc->debug_mode);

	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PARM0_DRV0_MODE0,
					mode_0_start_addr,
					rsc->debug_mode);

	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PARM0_DRV0_MODE1,
					mode_1_start_addr,
					rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PARM0_DRV0_MODE2,
					mode_2_start_addr,
					rsc->debug_mode);
	return 0;
}

static int _rsc_hw_solver_init(struct sde_rsc_priv *rsc)
{

	pr_debug("rsc solver init\n");

	dss_reg_w(&rsc->drv_io, SDE_RSCC_SOFT_WAKEUP_TIME_LO_DRV0,
					0xFFFFFFFF, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SOFT_WAKEUP_TIME_HI_DRV0,
					0xFFFFFFFF, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_MAX_IDLE_DURATION_DRV0,
					0xEFFFFFFF, rsc->debug_mode);

	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_TIME_SLOT_TABLE_0_DRV0,
						0x0, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_TIME_SLOT_TABLE_1_DRV0,
		rsc->timer_config.bwi_threshold_time_ns, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_TIME_SLOT_TABLE_2_DRV0,
		rsc->timer_config.rsc_time_slot_1_ns, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_TIME_SLOT_TABLE_3_DRV0,
		rsc->timer_config.rsc_time_slot_2_ns, rsc->debug_mode);

	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_SOLVER_MODES_ENABLED_DRV0,
						0x7, rsc->debug_mode);

	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PRI_TABLE_SLOT0_PRI0_DRV0,
						0x0, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PRI_TABLE_SLOT1_PRI0_DRV0,
						0x1, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PRI_TABLE_SLOT1_PRI3_DRV0,
						0x1, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PRI_TABLE_SLOT2_PRI0_DRV0,
						0x2, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PRI_TABLE_SLOT2_PRI3_DRV0,
						0x2, rsc->debug_mode);

	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_OVERRIDE_MODE_DRV0,
						0x0, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_TIMERS_CONSIDERED_DRV0,
						0x1, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_OVERRIDE_IDLE_TIME_DRV0,
						0x01000010, rsc->debug_mode);

	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PARM1_DRV0_MODE0,
					0x80000000, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PARM2_DRV0_MODE0,
			rsc->timer_config.rsc_backoff_time_ns, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PARM3_DRV0_MODE0,
			rsc->timer_config.pdc_backoff_time_ns, rsc->debug_mode);

	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PARM1_DRV0_MODE1,
					0x80000000, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PARM2_DRV0_MODE1,
			rsc->timer_config.rsc_backoff_time_ns * 2,
			rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PARM3_DRV0_MODE1,
			rsc->timer_config.pdc_backoff_time_ns, rsc->debug_mode);

	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PARM1_DRV0_MODE2,
					0x80000000, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PARM2_DRV0_MODE2,
					0x0, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PARM3_DRV0_MODE2,
			rsc->timer_config.pdc_backoff_time_ns, rsc->debug_mode);

	return 0;
}

static int sde_rsc_mode2_entry_trigger(struct sde_rsc_priv *rsc)
{
	int rc;
	int count, wrapper_status, ctrl2_status;
	unsigned long reg;

	/* update qtimers to high during clk & video mode state */
	if ((rsc->current_state == SDE_RSC_VID_STATE) ||
			(rsc->current_state == SDE_RSC_CLK_STATE)) {
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F0_QTMR_V1_CNTP_CVAL_HI,
						0xffffffff, rsc->debug_mode);
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F0_QTMR_V1_CNTP_CVAL_LO,
						0xffffffff, rsc->debug_mode);
	}

	wrapper_status = dss_reg_r(&rsc->wrapper_io, SDE_RSCC_WRAPPER_CTRL,
				rsc->debug_mode);
	wrapper_status |= BIT(3);
	wrapper_status |= BIT(0);
	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_CTRL,
					wrapper_status, rsc->debug_mode);

	ctrl2_status = dss_reg_r(&rsc->wrapper_io,
		SDE_RSCC_WRAPPER_OVERRIDE_CTRL2, rsc->debug_mode);
	ctrl2_status &= ~BIT(3);
	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL2,
		ctrl2_status, rsc->debug_mode);
	wmb(); /* make sure that vsync source is disabled */


	/**
	 * force busy and idle during clk & video mode state because it
	 * is trying to entry in mode-2 without turning on the vysnc.
	 */
	if ((rsc->current_state == SDE_RSC_VID_STATE) ||
			(rsc->current_state == SDE_RSC_CLK_STATE)) {
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL,
				BIT(0) | BIT(1), rsc->debug_mode);
		wmb(); /* force busy gurantee */
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL,
				BIT(0) | BIT(9), rsc->debug_mode);
	}

	wmb(); /* make sure that mode-2 is triggered before wait*/

	rc = -EBUSY;
	/* this wait is required to turn off the rscc clocks */
	for (count = MAX_CHECK_LOOPS; count > 0; count--) {
		reg = dss_reg_r(&rsc->wrapper_io,
				SDE_RSCC_PWR_CTRL, rsc->debug_mode);
		if (test_bit(POWER_CTRL_BIT_12, &reg)) {
			rc = 0;
			break;
		}
		usleep_range(50, 100);
	}

	return rc;
}

static void sde_rsc_reset_mode_0_1(struct sde_rsc_priv *rsc)
{
	u32 seq_busy, current_mode, curr_inst_addr;

	seq_busy = dss_reg_r(&rsc->drv_io, SDE_RSCC_SEQ_BUSY_DRV0,
			rsc->debug_mode);
	current_mode = dss_reg_r(&rsc->drv_io, SDE_RSCC_SOLVER_STATUS2_DRV0,
			rsc->debug_mode);
	curr_inst_addr = dss_reg_r(&rsc->drv_io, SDE_RSCC_SEQ_PROGRAM_COUNTER,
			rsc->debug_mode);
	SDE_EVT32(seq_busy, current_mode, curr_inst_addr);

	if (seq_busy && (current_mode == SDE_RSC_MODE_0_VAL ||
			current_mode == SDE_RSC_MODE_1_VAL)) {
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F1_QTMR_V1_CNTP_CVAL_HI,
						0xffffff, rsc->debug_mode);
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F1_QTMR_V1_CNTP_CVAL_LO,
						0xffffffff, rsc->debug_mode);
		wmb(); /* unstick f1 qtimer */

		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F1_QTMR_V1_CNTP_CVAL_HI,
						0x0, rsc->debug_mode);
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F1_QTMR_V1_CNTP_CVAL_LO,
						0x0, rsc->debug_mode);
		wmb(); /* manually trigger f1 qtimer interrupt */

		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F0_QTMR_V1_CNTP_CVAL_HI,
						0xffffff, rsc->debug_mode);
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F0_QTMR_V1_CNTP_CVAL_LO,
						0xffffffff, rsc->debug_mode);
		wmb(); /* unstick f0 qtimer */

		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F0_QTMR_V1_CNTP_CVAL_HI,
						0x0, rsc->debug_mode);
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F0_QTMR_V1_CNTP_CVAL_LO,
						0x0, rsc->debug_mode);
		wmb(); /* manually trigger f0 qtimer interrupt */
	}
}

static int sde_rsc_mode2_entry_v3(struct sde_rsc_priv *rsc)
{
	int rc = 0, i;
	u32 reg;

	if (rsc->power_collapse_block)
		return -EINVAL;

	if (rsc->sw_fs_enabled) {
		rc = regulator_set_mode(rsc->fs, REGULATOR_MODE_FAST);
		if (rc) {
			pr_err("vdd reg fast mode set failed rc:%d\n", rc);
			return rc;
		}
	}

	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_SOLVER_MODES_ENABLED_DRV0,
						0x7, rsc->debug_mode);

	/**
	 * increase delay time to wait before mode2 entry,
	 * longer time required subsequent to panel mode change
	 */
	if (rsc->post_poms)
		usleep_range(750, 1000);
	for (i = 0; i <= MAX_MODE2_ENTRY_TRY; i++) {
		rc = sde_rsc_mode2_entry_trigger(rsc);
		if (!rc)
			break;

		reg = dss_reg_r(&rsc->drv_io,
				SDE_RSCC_SEQ_PROGRAM_COUNTER, rsc->debug_mode);
		pr_err("mdss gdsc power down failed, instruction:0x%x, rc:%d\n",
				reg, rc);
		SDE_EVT32(rc, reg, SDE_EVTLOG_ERROR);

		/* avoid touching f1 qtimer for last try */
		if (i != MAX_MODE2_ENTRY_TRY)
			sde_rsc_reset_mode_0_1(rsc);
	}

	if (rc)
		goto end;

	if ((rsc->current_state == SDE_RSC_VID_STATE) ||
			(rsc->current_state == SDE_RSC_CLK_STATE)) {
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL,
					BIT(0) | BIT(8), rsc->debug_mode);
		wmb(); /* force busy on vsync */
	}

	if (rsc->sw_fs_enabled) {
		regulator_disable(rsc->fs);
		rsc->sw_fs_enabled = false;
	}

	return 0;

end:
	sde_rsc_mode2_exit(rsc, rsc->current_state);

	return rc;
}

static int sde_rsc_state_update_v3(struct sde_rsc_priv *rsc,
						enum sde_rsc_state state)
{
	int rc = 0;
	int reg, ctrl2_config;

	if (rsc->power_collapse) {
		rc = sde_rsc_mode2_exit(rsc, state);
		if (rc)
			pr_err("power collapse: mode2 exit failed\n");
		else
			rsc->power_collapse = false;
	}

	switch (state) {
	case SDE_RSC_CMD_STATE:
		pr_debug("command mode handling\n");

		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL,
							0x0, rsc->debug_mode);
		wmb(); /* disable double buffer config before vsync select */

		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL2,
				BIT(1) | BIT(2) | BIT(3), rsc->debug_mode);

		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_CTRL,
						0x1, rsc->debug_mode);
		dss_reg_w(&rsc->drv_io, SDE_RSCC_SOLVER_OVERRIDE_CTRL_DRV0,
							0x0, rsc->debug_mode);
		reg = dss_reg_r(&rsc->wrapper_io,
			SDE_RSCC_WRAPPER_OVERRIDE_CTRL, rsc->debug_mode);
		reg |= (BIT(0) | BIT(8));
		reg &= ~(BIT(1) | BIT(2) | BIT(3) | BIT(6) | BIT(7) | BIT(9));
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL,
							reg, rsc->debug_mode);
		wmb(); /* make sure that solver is enabled */

		break;

	case SDE_RSC_VID_STATE:
		pr_debug("video mode handling\n");

		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL,
							0x0, rsc->debug_mode);
		wmb(); /* disable double buffer config before vsync select */

		ctrl2_config = (rsc->vsync_source & 0x7) << 4;
		ctrl2_config |= (BIT(0) | BIT(1) | BIT(3));
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL2,
				ctrl2_config, rsc->debug_mode);
		wmb(); /* select vsync before double buffer config enabled */

		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_CTRL,
						0x1, rsc->debug_mode);
		dss_reg_w(&rsc->drv_io, SDE_RSCC_SOLVER_OVERRIDE_CTRL_DRV0,
							0x0, rsc->debug_mode);
		reg = dss_reg_r(&rsc->wrapper_io,
			SDE_RSCC_WRAPPER_OVERRIDE_CTRL, rsc->debug_mode);
		reg |= (BIT(0) | BIT(8));
		reg &= ~(BIT(1) | BIT(2) | BIT(3) | BIT(6) | BIT(7) | BIT(9));
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL,
							reg, rsc->debug_mode);
		wmb(); /* make sure that solver is enabled */

		break;

	case SDE_RSC_CLK_STATE:
		pr_debug("clk state handling\n");

		ctrl2_config = dss_reg_r(&rsc->wrapper_io,
			SDE_RSCC_WRAPPER_OVERRIDE_CTRL2, rsc->debug_mode);
		ctrl2_config &= ~(BIT(0) | BIT(1) | BIT(2));
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL2,
			ctrl2_config, rsc->debug_mode);

		reg = dss_reg_r(&rsc->wrapper_io,
			SDE_RSCC_WRAPPER_OVERRIDE_CTRL, rsc->debug_mode);
		reg &= ~(BIT(0) | BIT(8));
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL,
							reg, rsc->debug_mode);
		wmb(); /* make sure that solver mode is disabled */

		reg = dss_reg_r(&rsc->wrapper_io,
			SDE_RSCC_WRAPPER_OVERRIDE_CTRL, rsc->debug_mode);
		reg |= BIT(8);
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL,
							reg, rsc->debug_mode);
		wmb(); /* enable double buffer vsync configuration */
		break;

	case SDE_RSC_IDLE_STATE:
		rc = sde_rsc_mode2_entry_v3(rsc);
		if (rc)
			pr_err("power collapse - mode 2 entry failed\n");
		else
			rsc->power_collapse = true;
		break;

	default:
		pr_err("state:%d handling is not supported\n", state);
		break;
	}

	return rc;
}

int rsc_hw_init_v3(struct sde_rsc_priv *rsc)
{
	int rc = 0;

	rsc->hw_drv_ver = dss_reg_r(&rsc->drv_io,
		SDE_RSCC_RSC_ID_DRV0, rsc->debug_mode);

	rc = _rsc_hw_qtimer_init(rsc);
	if (rc) {
		pr_err("rsc hw qtimer init failed\n");
		goto end;
	}

	rc = _rsc_hw_wrapper_init(rsc);
	if (rc) {
		pr_err("rsc hw wrapper init failed\n");
		goto end;
	}

	rc = _rsc_hw_seq_memory_init_v3(rsc);
	if (rc) {
		pr_err("rsc sequencer memory init failed\n");
		goto end;
	}

	rc = _rsc_hw_solver_init(rsc);
	if (rc) {
		pr_err("rsc solver init failed\n");
		goto end;
	}

	rc = _rsc_hw_pdc_init(rsc);
	if (rc) {
		pr_err("rsc hw pdc init failed\n");
		goto end;
	}

	wmb(); /* make sure that hw is initialized */

	pr_info("sde rsc init successfully done\n");
end:
	return rc;
}

int rsc_hw_bwi_status_v3(struct sde_rsc_priv *rsc, bool bw_indication)
{
	int count, bw_ack;
	int rc = 0;

	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_BW_INDICATION,
						bw_indication, rsc->debug_mode);
	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_CTRL,
						0x1, rsc->debug_mode);

	bw_ack = dss_reg_r(&rsc->wrapper_io, SDE_RSCC_WRAPPER_DEBUG_CTRL2,
			rsc->debug_mode) & BIT(14);

	/* check for sequence running status before exiting */
	for (count = MAX_CHECK_LOOPS; count > 0 && !bw_ack; count--) {
		usleep_range(8, 10);

		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_BW_INDICATION,
						bw_indication, rsc->debug_mode);
		bw_ack = dss_reg_r(&rsc->wrapper_io,
		       SDE_RSCC_WRAPPER_DEBUG_CTRL2, rsc->debug_mode) & BIT(14);
	}

	if (!bw_ack)
		rc = -EINVAL;

	return rc;
}

static int rsc_hw_profiling_counter_ctrl(struct sde_rsc_priv *rsc, bool enable)
{
	int i;

	if (!rsc) {
		pr_debug("invalid input param\n");
		return -EINVAL;
	}

	for (i = 0; i < NUM_RSC_PROFILING_COUNTERS; ++i) {
		dss_reg_w(&rsc->drv_io,
			SDE_RSCC_LPM_PROFILING_COUNTER0_EN_DRV0 +
			(0x20 * i), enable ? 1 : 0, rsc->debug_mode);
		dss_reg_w(&rsc->drv_io,
			SDE_RSCC_LPM_PROFILING_COUNTER0_CLR_DRV0 +
			(0x20 * i), 1, rsc->debug_mode);
	}

	wmb(); /* make sure counters are cleared now */
	pr_debug("rsc profiling counters %s and cleared\n",
			enable ? "enabled" : "disabled");

	return 0;
}

static int rsc_hw_get_profiling_counter_status(struct sde_rsc_priv *rsc,
		u32 *counters)
{
	int i;

	if (!rsc || !counters) {
		pr_debug("invalid input param, %d %d\n",
				rsc ? 0 : 1, counters ? 0 : 1);
		return -EINVAL;
	}

	for (i = 0; i < NUM_RSC_PROFILING_COUNTERS; ++i)
		counters[i] = dss_reg_r(&rsc->drv_io,
				SDE_RSCC_LPM_PROFILING_COUNTER0_STATUS_DRV0 +
				(0x20 * i), rsc->debug_mode);

	return 0;
}

static int rsc_hw_timer_update_v3(struct sde_rsc_priv *rsc)
{
	if (!rsc) {
		pr_debug("invalid input param\n");
		return -EINVAL;
	}

	pr_debug("rsc hw timer update\n");

	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_TIME_SLOT_TABLE_1_DRV0,
		rsc->timer_config.rsc_time_slot_0_ns, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_TIME_SLOT_TABLE_2_DRV0,
		rsc->timer_config.rsc_time_slot_1_ns, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_TIME_SLOT_TABLE_3_DRV0,
		rsc->timer_config.rsc_time_slot_2_ns, rsc->debug_mode);

	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PARM2_DRV0_MODE0,
			rsc->timer_config.rsc_backoff_time_ns, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PARM3_DRV0_MODE0,
			rsc->timer_config.pdc_backoff_time_ns, rsc->debug_mode);

	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PARM2_DRV0_MODE1,
			rsc->timer_config.rsc_backoff_time_ns * 2,
			rsc->debug_mode);

	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PARM3_DRV0_MODE1,
			rsc->timer_config.pdc_backoff_time_ns, rsc->debug_mode);

	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_MODE_PARM3_DRV0_MODE2,
			rsc->timer_config.pdc_backoff_time_ns, rsc->debug_mode);

	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_STATIC_WAKEUP_0,
		rsc->timer_config.static_wakeup_time_ns, rsc->debug_mode);

	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_RSCC_MODE_THRESHOLD,
		rsc->timer_config.rsc_mode_threshold_time_ns, rsc->debug_mode);

	/* make sure that hw timers are updated */
	wmb();

	return 0;
}

int sde_rsc_hw_register_v3(struct sde_rsc_priv *rsc)
{
	pr_debug("rsc hardware register v3\n");

	rsc->hw_ops.init = rsc_hw_init_v3;
	rsc->hw_ops.state_update = sde_rsc_state_update_v3;
	rsc->hw_ops.bwi_status = rsc_hw_bwi_status_v3;
	rsc->hw_ops.timer_update = rsc_hw_timer_update_v3;

	rsc->hw_ops.tcs_wait = rsc_hw_tcs_wait;
	rsc->hw_ops.tcs_use_ok = rsc_hw_tcs_use_ok;
	rsc->hw_ops.is_amc_mode = rsc_hw_is_amc_mode;
	rsc->hw_ops.hw_vsync = rsc_hw_vsync;
	rsc->hw_ops.debug_show = sde_rsc_debug_show;
	rsc->hw_ops.mode_ctrl = rsc_hw_mode_ctrl;
	rsc->hw_ops.debug_dump = rsc_hw_debug_dump;
	if (rsc->profiling_supp) {
		rsc->hw_ops.setup_counters = rsc_hw_profiling_counter_ctrl;
		rsc->hw_ops.get_counters = rsc_hw_get_profiling_counter_status;
	}

	return 0;
}
