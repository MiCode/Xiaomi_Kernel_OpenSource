/* Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"[sde_rsc_hw:%s:%d]: " fmt, __func__, __LINE__

#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/delay.h>

#include "sde_rsc_priv.h"
#include "sde_dbg.h"
#include "sde_rsc_hw.h"

static void rsc_event_trigger(struct sde_rsc_priv *rsc, uint32_t event_type)
{
	struct sde_rsc_event *event;

	list_for_each_entry(event, &rsc->event_list, list)
		if (event->event_type & event_type)
			event->cb_func(event_type, event->usr);
}

static int rsc_hw_qtimer_init(struct sde_rsc_priv *rsc)
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

static int rsc_hw_pdc_init(struct sde_rsc_priv *rsc)
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

static int rsc_hw_wrapper_init(struct sde_rsc_priv *rsc)
{
	pr_debug("rsc hardware wrapper init\n");

	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_STATIC_WAKEUP_0,
		rsc->timer_config.static_wakeup_time_ns, rsc->debug_mode);

	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_RSCC_MODE_THRESHOLD,
		rsc->timer_config.rsc_mode_threshold_time_ns, rsc->debug_mode);

	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL,
						BIT(8), rsc->debug_mode);
	return 0;
}

static int rsc_hw_seq_memory_init_v2(struct sde_rsc_priv *rsc)
{
	const u32 mode_0_start_addr = 0x0;
	const u32 mode_1_start_addr = 0xc;
	const u32 mode_2_start_addr = 0x18;

	pr_debug("rsc sequencer memory init v2\n");

	/* Mode - 0 sequence */
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x0,
						0xe0bb9ebe, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x4,
						0x9ebeff39, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x8,
						0x2020209b, rsc->debug_mode);

	/* Mode - 1 sequence */
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0xc,
						0x38bb9ebe, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x10,
						0xbeff39e0, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x14,
						0x20209b9e, rsc->debug_mode);

	/* Mode - 2 sequence */
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x18,
						0xb9bae5a0, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x1c,
						0xbdbbf9fa, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x20,
						0x38999afe, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x24,
						0xac81e1a1, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x28,
						0x82e2a2e0, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x2c,
						0x8cfd9d39, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x30,
						0xbc20209b, rsc->debug_mode);

	/* tcs sleep & wake sequence */
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x34,
						0xe601a6fc, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x38,
						0xbc20209c, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x3c,
						0xe701a7fc, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x40,
						0x0000209c, rsc->debug_mode);

	/* branch address */
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_CFG_BR_ADDR_0_DRV0,
						0x33, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_CFG_BR_ADDR_1_DRV0,
						0x3b, rsc->debug_mode);

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
static int rsc_hw_seq_memory_init(struct sde_rsc_priv *rsc)
{
	const u32 mode_0_start_addr = 0x0;
	const u32 mode_1_start_addr = 0xa;
	const u32 mode_2_start_addr = 0x15;

	pr_debug("rsc sequencer memory init\n");

	/* Mode - 0 sequence */
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x0,
						0xe0a88bab, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x4,
						0x8babec39, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x8,
						0x8bab2088, rsc->debug_mode);

	/* Mode - 1 sequence */
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0xc,
						0x39e038a8, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x10,
						0x888babec, rsc->debug_mode);

	/* Mode - 2 sequence */
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x14,
						0xaaa8a020, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x18,
						0xe1a138eb, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x1c,
						0xe0aca581, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x20,
						0x82e2a2ed, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x24,
						0x8cea8a39, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x28,
						0xe9a92088, rsc->debug_mode);

	/* tcs sleep & wake sequence */
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x2c,
						0x89e686a6, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x30,
						0xa7e9a920, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_MEM_0_DRV0 + 0x34,
						0x2089e787, rsc->debug_mode);

	/* branch address */
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_CFG_BR_ADDR_0_DRV0,
						0x2a, rsc->debug_mode);
	dss_reg_w(&rsc->drv_io, SDE_RSCC_SEQ_CFG_BR_ADDR_1_DRV0,
						0x31, rsc->debug_mode);

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

static int rsc_hw_solver_init(struct sde_rsc_priv *rsc)
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
		rsc->timer_config.rsc_time_slot_0_ns, rsc->debug_mode);
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
			rsc->timer_config.rsc_backoff_time_ns, rsc->debug_mode);
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

static int rsc_hw_timer_update(struct sde_rsc_priv *rsc)
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
			rsc->timer_config.rsc_backoff_time_ns, rsc->debug_mode);
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

int sde_rsc_mode2_exit(struct sde_rsc_priv *rsc, enum sde_rsc_state state)
{
	int rc = -EBUSY;
	int count, reg;
	unsigned long power_status;

	rsc_event_trigger(rsc, SDE_RSC_EVENT_PRE_CORE_RESTORE);

	/**
	 * force busy and idle during clk & video mode state because it
	 * is trying to entry in mode-2 without turning on the vysnc.
	 */
	if ((state == SDE_RSC_VID_STATE) || (state == SDE_RSC_CLK_STATE)) {
		reg = dss_reg_r(&rsc->wrapper_io,
			SDE_RSCC_WRAPPER_OVERRIDE_CTRL, rsc->debug_mode);
		reg &= ~(BIT(8) | BIT(0));
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL,
							reg, rsc->debug_mode);
	}

	// needs review with HPG sequence
	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F1_QTMR_V1_CNTP_CVAL_LO,
					0x0, rsc->debug_mode);
	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F1_QTMR_V1_CNTP_CVAL_HI,
					0x0, rsc->debug_mode);

	reg = dss_reg_r(&rsc->wrapper_io, SDE_RSCC_WRAPPER_CTRL,
				rsc->debug_mode);
	reg &= ~BIT(3);
	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_CTRL,
					reg, rsc->debug_mode);

	if (rsc->version < SDE_RSC_REV_2) {
		reg = dss_reg_r(&rsc->wrapper_io, SDE_RSCC_SPARE_PWR_EVENT,
							rsc->debug_mode);
		reg |= BIT(13);
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_SPARE_PWR_EVENT,
							reg, rsc->debug_mode);
	}

	/* make sure that mode-2 exit before wait*/
	wmb();

	/* this wait is required to make sure that gdsc is powered on */
	for (count = MAX_CHECK_LOOPS; count > 0; count--) {
		power_status = dss_reg_r(&rsc->wrapper_io,
				SDE_RSCC_PWR_CTRL, rsc->debug_mode);
		if (!test_bit(POWER_CTRL_BIT_12, &power_status)) {
			reg = dss_reg_r(&rsc->drv_io,
				SDE_RSCC_SEQ_PROGRAM_COUNTER, rsc->debug_mode);
			SDE_EVT32_VERBOSE(count, reg, power_status);
			rc = 0;
			break;
		}
		usleep_range(10, 100);
	}

	if (rsc->version < SDE_RSC_REV_2) {
		reg = dss_reg_r(&rsc->wrapper_io, SDE_RSCC_SPARE_PWR_EVENT,
							rsc->debug_mode);
		reg &= ~BIT(13);
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_SPARE_PWR_EVENT,
							reg, rsc->debug_mode);
	}

	if (rc)
		pr_err("vdd reg is not enabled yet\n");

	dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_SOLVER_MODES_ENABLED_DRV0,
						0x3, rsc->debug_mode);

	if (rsc->version >= SDE_RSC_REV_3) {
		reg = dss_reg_r(&rsc->wrapper_io,
			SDE_RSCC_WRAPPER_OVERRIDE_CTRL, rsc->debug_mode);
		reg &= ~(BIT(0) | BIT(8));
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL,
						reg, rsc->debug_mode);
		wmb(); /* make sure to disable rsc solver state */

		reg = dss_reg_r(&rsc->wrapper_io,
			SDE_RSCC_WRAPPER_OVERRIDE_CTRL, rsc->debug_mode);
		reg |= (BIT(0) | BIT(8));
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL,
						reg, rsc->debug_mode);
		wmb(); /* make sure to enable rsc solver state */
	}

	rsc_event_trigger(rsc, SDE_RSC_EVENT_POST_CORE_RESTORE);

	return rc;
}

static int sde_rsc_mode2_entry_trigger(struct sde_rsc_priv *rsc)
{
	int rc;
	int count, wrapper_status;
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

	/* make sure that mode-2 is triggered before wait*/
	wmb();

	rc = -EBUSY;
	/* this wait is required to turn off the rscc clocks */
	for (count = MAX_CHECK_LOOPS; count > 0; count--) {
		reg = dss_reg_r(&rsc->wrapper_io,
				SDE_RSCC_PWR_CTRL, rsc->debug_mode);
		if (test_bit(POWER_CTRL_BIT_12, &reg)) {
			rc = 0;
			break;
		}
		usleep_range(10, 100);
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
		/* unstick f1 qtimer */
		wmb();

		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F1_QTMR_V1_CNTP_CVAL_HI,
						0x0, rsc->debug_mode);
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F1_QTMR_V1_CNTP_CVAL_LO,
						0x0, rsc->debug_mode);
		/* manually trigger f1 qtimer interrupt */
		wmb();

		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F0_QTMR_V1_CNTP_CVAL_HI,
						0xffffff, rsc->debug_mode);
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F0_QTMR_V1_CNTP_CVAL_LO,
						0xffffffff, rsc->debug_mode);
		/* unstick f0 qtimer */
		wmb();

		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F0_QTMR_V1_CNTP_CVAL_HI,
						0x0, rsc->debug_mode);
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_F0_QTMR_V1_CNTP_CVAL_LO,
						0x0, rsc->debug_mode);
		/* manually trigger f0 qtimer interrupt */
		wmb();
	}
}

static int sde_rsc_mode2_entry(struct sde_rsc_priv *rsc)
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
	rsc_event_trigger(rsc, SDE_RSC_EVENT_PRE_CORE_PC);

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

	rsc_event_trigger(rsc, SDE_RSC_EVENT_POST_CORE_PC);

	if (rsc->sw_fs_enabled) {
		regulator_disable(rsc->fs);
		rsc->sw_fs_enabled = false;
	}

	return 0;

end:
	sde_rsc_mode2_exit(rsc, rsc->current_state);

	return rc;
}

static int sde_rsc_state_update(struct sde_rsc_priv *rsc,
						enum sde_rsc_state state)
{
	int rc = 0;
	int reg;

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
		/* make sure that solver is enabled */
		wmb();

		rsc_event_trigger(rsc, SDE_RSC_EVENT_SOLVER_ENABLED);
		break;

	case SDE_RSC_VID_STATE:
		pr_debug("video mode handling\n");

		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_CTRL,
						0x1, rsc->debug_mode);
		reg = dss_reg_r(&rsc->wrapper_io,
			SDE_RSCC_WRAPPER_OVERRIDE_CTRL, rsc->debug_mode);
		reg |= BIT(8);
		reg &= ~(BIT(1) | BIT(0));
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL,
							reg, rsc->debug_mode);
		/* make sure that solver mode is override */
		wmb();

		rsc_event_trigger(rsc, SDE_RSC_EVENT_SOLVER_DISABLED);
		break;

	case SDE_RSC_CLK_STATE:
		pr_debug("clk state handling\n");

		reg = dss_reg_r(&rsc->wrapper_io,
			SDE_RSCC_WRAPPER_OVERRIDE_CTRL, rsc->debug_mode);
		reg &= ~BIT(0);
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL,
							reg, rsc->debug_mode);
		/* make sure that solver mode is disabled */
		wmb();
		break;

	case SDE_RSC_IDLE_STATE:
		rc = sde_rsc_mode2_entry(rsc);
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

int rsc_hw_init(struct sde_rsc_priv *rsc)
{
	int rc = 0;

	rc = rsc_hw_qtimer_init(rsc);
	if (rc) {
		pr_err("rsc hw qtimer init failed\n");
		goto end;
	}

	rc = rsc_hw_wrapper_init(rsc);
	if (rc) {
		pr_err("rsc hw wrapper init failed\n");
		goto end;
	}

	if (rsc->version == SDE_RSC_REV_2)
		rc = rsc_hw_seq_memory_init_v2(rsc);
	else
		rc = rsc_hw_seq_memory_init(rsc);
	if (rc) {
		pr_err("rsc sequencer memory init failed\n");
		goto end;
	}

	rc = rsc_hw_solver_init(rsc);
	if (rc) {
		pr_err("rsc solver init failed\n");
		goto end;
	}

	rc = rsc_hw_pdc_init(rsc);
	if (rc) {
		pr_err("rsc hw pdc init failed\n");
		goto end;
	}

	/* make sure that hw is initialized */
	wmb();

	pr_info("sde rsc init successfully done\n");
end:
	return rc;
}

int rsc_hw_mode_ctrl(struct sde_rsc_priv *rsc, enum rsc_mode_req request,
		char *buffer, int buffer_size, u32 mode)
{
	u32 blen = 0;
	u32 slot_time;

	switch (request) {
	case MODE_READ:
		if (!buffer || !buffer_size)
			return blen;

		blen = snprintf(buffer, buffer_size - blen,
			"mode_status:0x%x\n",
			dss_reg_r(&rsc->drv_io, SDE_RSCC_SOLVER_STATUS2_DRV0,
			rsc->debug_mode));
		break;

	case MODE_UPDATE:
		slot_time = mode & BIT(0) ? 0x0 :
					rsc->timer_config.rsc_time_slot_2_ns;
		dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_TIME_SLOT_TABLE_0_DRV0,
						slot_time, rsc->debug_mode);

		slot_time = mode & BIT(1) ?
			rsc->timer_config.rsc_time_slot_0_ns :
				rsc->timer_config.rsc_time_slot_2_ns;
		dss_reg_w(&rsc->drv_io, SDE_RSC_SOLVER_TIME_SLOT_TABLE_1_DRV0,
						slot_time, rsc->debug_mode);

		rsc->power_collapse_block = !(mode & BIT(2));
		break;

	default:
		break;
	}

	return blen;
}

int sde_rsc_debug_show(struct seq_file *s, struct sde_rsc_priv *rsc)
{
	seq_printf(s, "override ctrl:0x%x\n",
		 dss_reg_r(&rsc->wrapper_io, SDE_RSCC_WRAPPER_OVERRIDE_CTRL,
				rsc->debug_mode));
	seq_printf(s, "power ctrl:0x%x\n",
		 dss_reg_r(&rsc->wrapper_io, SDE_RSCC_PWR_CTRL,
				rsc->debug_mode));
	seq_printf(s, "vsycn timestamp0:0x%x\n",
		 dss_reg_r(&rsc->wrapper_io, SDE_RSCC_WRAPPER_VSYNC_TIMESTAMP0,
				rsc->debug_mode));
	seq_printf(s, "vsycn timestamp1:0x%x\n",
		 dss_reg_r(&rsc->wrapper_io, SDE_RSCC_WRAPPER_VSYNC_TIMESTAMP1,
				rsc->debug_mode));

	seq_printf(s, "error irq status:0x%x\n",
		 dss_reg_r(&rsc->drv_io, SDE_RSCC_ERROR_IRQ_STATUS_DRV0,
				rsc->debug_mode));

	seq_printf(s, "seq busy status:0x%x\n",
		 dss_reg_r(&rsc->drv_io, SDE_RSCC_SEQ_BUSY_DRV0,
				rsc->debug_mode));

	seq_printf(s, "solver override ctrl status:0x%x\n",
		 dss_reg_r(&rsc->drv_io, SDE_RSCC_SOLVER_OVERRIDE_CTRL_DRV0,
				rsc->debug_mode));
	seq_printf(s, "solver override status:0x%x\n",
		 dss_reg_r(&rsc->drv_io, SDE_RSCC_SOLVER_STATUS0_DRV0,
				rsc->debug_mode));
	seq_printf(s, "solver timeslot status:0x%x\n",
		 dss_reg_r(&rsc->drv_io, SDE_RSCC_SOLVER_STATUS1_DRV0,
				rsc->debug_mode));
	seq_printf(s, "solver mode status:0x%x\n",
		 dss_reg_r(&rsc->drv_io, SDE_RSCC_SOLVER_STATUS2_DRV0,
				rsc->debug_mode));

	seq_printf(s, "amc status:0x%x\n",
		 dss_reg_r(&rsc->drv_io, SDE_RSCC_AMC_TCS_MODE_IRQ_STATUS_DRV0,
				rsc->debug_mode));

	return 0;
}

int rsc_hw_vsync(struct sde_rsc_priv *rsc, enum rsc_vsync_req request,
		char *buffer, int buffer_size, u32 mode)
{
	u32 blen = 0, reg;

	switch (request) {
	case VSYNC_READ:
		if (!buffer || !buffer_size)
			return blen;

		blen = snprintf(buffer, buffer_size - blen, "vsync0:0x%x\n",
			 dss_reg_r(&rsc->wrapper_io,
				SDE_RSCC_WRAPPER_VSYNC_TIMESTAMP0,
				rsc->debug_mode));
		if (blen >= buffer_size)
			return blen;

		blen += snprintf(buffer + blen, buffer_size - blen,
			"vsync1:0x%x\n",
			 dss_reg_r(&rsc->wrapper_io,
				SDE_RSCC_WRAPPER_VSYNC_TIMESTAMP1,
				rsc->debug_mode));
		break;

	case VSYNC_READ_VSYNC0:
		return dss_reg_r(&rsc->wrapper_io,
				SDE_RSCC_WRAPPER_VSYNC_TIMESTAMP0,
				rsc->debug_mode);

	case VSYNC_ENABLE:
		/* clear the current VSYNC value */
		reg = BIT(9) | ((mode & 0x7) << 10);
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_DEBUG_BUS,
					reg, rsc->debug_mode);

		/* enable the VSYNC logging */
		reg = BIT(8) | ((mode & 0x7) << 10);
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_DEBUG_BUS,
				reg, rsc->debug_mode);

		/* ensure vsync config has been written before waiting on it */
		wmb();
		break;

	case VSYNC_DISABLE:
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_DEBUG_BUS,
						0x0, rsc->debug_mode);
		break;
	}

	return blen;
}

void rsc_hw_debug_dump(struct sde_rsc_priv *rsc, u32 mux_sel)
{
	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_DEBUG_BUS,
		((mux_sel & 0xf) << 1) | BIT(0), rsc->debug_mode);
}

bool rsc_hw_is_amc_mode(struct sde_rsc_priv *rsc)
{
	return dss_reg_r(&rsc->drv_io, SDE_RSCC_TCS_DRV0_CONTROL,
			rsc->debug_mode) & BIT(16);
}

int rsc_hw_tcs_wait(struct sde_rsc_priv *rsc)
{
	int rc = -EBUSY;
	int count, seq_status;

	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_CTRL,
						0x0, rsc->debug_mode);
	seq_status = dss_reg_r(&rsc->wrapper_io, SDE_RSCC_WRAPPER_CTRL,
			rsc->debug_mode) & BIT(1);
	/* if seq busy - set TCS use OK to high and wait for 200us */
	if (seq_status) {
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_CTRL,
						0x1, rsc->debug_mode);
		usleep_range(100, 200);
		dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_CTRL,
						0x0, rsc->debug_mode);
	}

	/* check for sequence running status before exiting */
	for (count = MAX_CHECK_LOOPS; count > 0; count--) {
		seq_status = dss_reg_r(&rsc->wrapper_io, SDE_RSCC_WRAPPER_CTRL,
				rsc->debug_mode) & BIT(1);
		if (!seq_status) {
			rc = 0;
			break;
		}
		usleep_range(1, 2);
	}

	return rc;
}

int rsc_hw_tcs_use_ok(struct sde_rsc_priv *rsc)
{
	dss_reg_w(&rsc->wrapper_io, SDE_RSCC_WRAPPER_CTRL,
						0x1, rsc->debug_mode);
	return 0;
}

int sde_rsc_hw_register(struct sde_rsc_priv *rsc)
{
	pr_debug("rsc hardware register\n");

	rsc->hw_ops.init = rsc_hw_init;
	rsc->hw_ops.timer_update = rsc_hw_timer_update;

	rsc->hw_ops.tcs_wait = rsc_hw_tcs_wait;
	rsc->hw_ops.tcs_use_ok = rsc_hw_tcs_use_ok;
	rsc->hw_ops.is_amc_mode = rsc_hw_is_amc_mode;

	rsc->hw_ops.hw_vsync = rsc_hw_vsync;
	rsc->hw_ops.state_update = sde_rsc_state_update;
	rsc->hw_ops.debug_show = sde_rsc_debug_show;
	rsc->hw_ops.mode_ctrl = rsc_hw_mode_ctrl;
	rsc->hw_ops.debug_dump = rsc_hw_debug_dump;

	return 0;
}
