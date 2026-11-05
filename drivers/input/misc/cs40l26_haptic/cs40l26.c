// SPDX-License-Identifier: GPL-2.0
//
// cs40l26.c -- CS40L26 Boosted Haptic Driver with Integrated DSP and
// Waveform Memory with Advanced Closed Loop Algorithms and LRA protection
//
// Copyright 2022 Cirrus Logic, Inc.
//
// Author: Fred Treven <fred.treven@cirrus.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.
#define DEBUG
#define BOOTUP_VIBE_FROM_RAM

#include <linux/mfd/cs40l26.h>

static const struct cs40l26_rom_regs cs40l26_rom_regs_a1_b0_b1 = {
	.pm_cur_state = 0x02800370,
	.pm_state_locks = 0x02800378,
	.pm_timeout_ticks = 0x02800350,
	.dsp_halo_state = 0x02800fa8,
	.event_map_table_event_data_packed = 0x02806FC4,
	.p_vibegen_rom = 0x02802154,
	.rom_pseq_end_of_script = 0x028003E8,
};

static const struct cs40l26_rom_data cs40l26_rom_data_a1_b0_b1 = {
	.rom_wt_size_words = 1549,
	.rom_num_waves = 39,
};

static const struct cs40l26_rom_regs cs40l26_rom_regs_b2 = { /* RC2 8.1.2 */
	.pm_cur_state = 0x02801F98,
	.pm_state_locks = 0x02801FA0,
	.pm_timeout_ticks = 0x02801F78,
	.dsp_halo_state = 0x02806AF8,
	.event_map_table_event_data_packed = 0x02806FB0,
	.p_vibegen_rom = 0x02802F50,
	.rom_pseq_end_of_script = 0x02802018,
};

static const struct cs40l26_rom_data cs40l26_rom_data_b2 = {
	.rom_wt_size_words = 1549,
	.rom_num_waves = 39,
};

static inline bool section_complete(struct cs40l26_owt_section *s)
{
	return s->delay ? true : false;
}

static u32 gpio_map_get(struct device *dev, enum cs40l26_gpio_map gpio)
{
	const char *name = (gpio == CS40L26_GPIO_MAP_A_PRESS) ?
			"cirrus,press-index" : "cirrus,release-index";
	u32 bank_idx_pair[2];
	int error;

	error = device_property_read_u32_array(dev, name, bank_idx_pair, 2);
	if (error)
		return CS40L26_EVENT_MAP_GPI_DISABLE;

	if (bank_idx_pair[0] == CS40L26_RAM_BANK_ID)
		return (bank_idx_pair[1] & CS40L26_BTN_INDEX_MASK) | (1 << CS40L26_BTN_BANK_SHIFT);
	else if (bank_idx_pair[0] == CS40L26_ROM_BANK_ID)
		return (bank_idx_pair[1] & CS40L26_BTN_INDEX_MASK);

	return CS40L26_EVENT_MAP_GPI_DISABLE;
}

static int cs40l26_dsp_read(struct cs40l26_private *cs40l26, u32 reg, u32 *val)
{
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	u32 read_val;
	int i;

	for (i = 0; i < CS40L26_DSP_TIMEOUT_COUNT; i++) {
		if (regmap_read(regmap, reg, &read_val))
			dev_dbg(dev, "Failed to read 0x%X, attempt(s) = %d\n", reg, i + 1);
		else
			break;

		usleep_range(CS40L26_DSP_TIMEOUT_US_MIN, CS40L26_DSP_TIMEOUT_US_MAX);
	}

	if (i >= CS40L26_DSP_TIMEOUT_COUNT) {
		dev_err(dev, "Timed out attempting to read 0x%X\n", reg);
		return -ETIME;
	}

	*val = read_val;

	return 0;
}

static int cs40l26_dsp_write(struct cs40l26_private *cs40l26, u32 reg, u32 val)
{
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	int i;

	for (i = 0; i < CS40L26_DSP_TIMEOUT_COUNT; i++) {
		if (regmap_write(regmap, reg, val))
			dev_dbg(dev, "Failed to write to 0x%X, attempt(s) = %d\n", reg, i + 1);
		else
			break;

		usleep_range(CS40L26_DSP_TIMEOUT_US_MIN, CS40L26_DSP_TIMEOUT_US_MAX);
	}

	if (i >= CS40L26_DSP_TIMEOUT_COUNT) {
		dev_err(dev, "Timed out attempting to write to 0x%X\n", reg);
		return -ETIME;
	}

	return 0;
}

int cs40l26_get_ram_ext_algo_id(struct cs40l26_private *cs40l26, unsigned int *algo_id)
{
	unsigned int fw_rev;
	int error;

	error = cl_dsp_fw_rev_get(cs40l26->dsp, &fw_rev);
	if (error)
		return error;

	*algo_id = (fw_rev >= CS40L26_FW_RAM_EXT_RELOC_REV) ? cs40l26->fw_id : CS40L26_EXT_ALGO_ID;

	return 0;
}
EXPORT_SYMBOL_GPL(cs40l26_get_ram_ext_algo_id);

int cs40l26_mailbox_write(struct cs40l26_private *cs40l26, u32 write_val)
{
	int i, error;
	u32 val;

	dev_dbg(cs40l26->dev, "%s: cmd = 0x%08X\n", __func__, write_val);
	
	error = cs40l26_dsp_write(cs40l26, CS40L26_DSP_VIRTUAL1_MBOX_1, write_val);
	if (error)
		return error;

	for (i = 0; i < CS40L26_DSP_TIMEOUT_COUNT; i++) {
		error = cs40l26_dsp_read(cs40l26, CS40L26_DSP_VIRTUAL1_MBOX_1, &val);
		if (error)
			return error;

		if (val == 0x0)
			break;

		usleep_range(CS40L26_DSP_TIMEOUT_US_MIN, CS40L26_DSP_TIMEOUT_US_MAX);
	}

	if (i >= CS40L26_DSP_TIMEOUT_COUNT) {
		dev_err(cs40l26->dev, "Mailbox not acknowledged (0x%08X != 0x0)\n", val);
		return -ETIMEDOUT;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(cs40l26_mailbox_write);

int cs40l26_dsp_state_get(struct cs40l26_private *cs40l26, u8 *state)
{
	u32 reg, dsp_state;
	int error;

	if (cs40l26->fw_loaded) {
		error = cl_dsp_get_reg(cs40l26->dsp, "PM_CUR_STATE", CL_DSP_XM_UNPACKED_TYPE,
				CS40L26_PM_ALGO_ID, &reg);
		if (error)
			return error;
	} else {
		reg = cs40l26->rom_regs->pm_cur_state;
	}

	error = cs40l26_dsp_read(cs40l26, reg, &dsp_state);
	if (error)
		return error;

	switch (dsp_state) {
	case CS40L26_DSP_STATE_HIBERNATE:
		/* intentionally fall through */
	case CS40L26_DSP_STATE_SHUTDOWN:
		/* intentionally fall through */
	case CS40L26_DSP_STATE_STANDBY:
		/* intentionally fall through */
	case CS40L26_DSP_STATE_ACTIVE:
		*state = CS40L26_DSP_STATE_MASK & dsp_state;
		break;
	default:
		dev_err(cs40l26->dev, "DSP state %u is invalid\n", dsp_state);
		error = -EINVAL;
	}

	return error;
}
EXPORT_SYMBOL_GPL(cs40l26_dsp_state_get);

int cs40l26_set_pll_loop(struct cs40l26_private *cs40l26, unsigned int pll_loop)
{
	int i;

	dev_dbg(cs40l26->dev, "%s: pll_loop = %u\n", __func__, pll_loop);
	
	if (pll_loop != CS40L26_PLL_REFCLK_SET_OPEN_LOOP &&
			pll_loop != CS40L26_PLL_REFCLK_SET_CLOSED_LOOP) {
		dev_err(cs40l26->dev, "Invalid PLL Loop setting: %u\n", pll_loop);
		return -EINVAL;
	}

	/* Retry in case DSP is hibernating */
	for (i = 0; i < CS40L26_PLL_REFCLK_SET_ATTEMPTS; i++) {
		if (!regmap_update_bits(cs40l26->regmap, CS40L26_REFCLK_INPUT,
				CS40L26_PLL_REFCLK_LOOP_MASK, pll_loop <<
				CS40L26_PLL_REFCLK_LOOP_SHIFT))
			break;
	}

	if (i == CS40L26_PLL_REFCLK_SET_ATTEMPTS) {
		dev_err(cs40l26->dev, "Failed to configure PLL\n");
		return -ETIMEDOUT;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(cs40l26_set_pll_loop);

static const struct cs40l26_dbc cs40l26_dbc_params[CS40L26_DBC_NUM_CONTROLS] = {
	{
		.name = CS40L26_DBC_ENV_REL_COEF_NAME,
		.max = CS40L26_DBC_CONTROLS_MAX,
	},
	{
		.name = CS40L26_DBC_RISE_HEADROOM_NAME,
		.max = CS40L26_DBC_CONTROLS_MAX,
	},
	{
		.name = CS40L26_DBC_FALL_HEADROOM_NAME,
		.max = CS40L26_DBC_CONTROLS_MAX,
	},
	{
		.name = CS40L26_DBC_TX_LVL_THRESH_FS_NAME,
		.max = CS40L26_DBC_CONTROLS_MAX,
	},
	{
		.name = CS40L26_DBC_TX_LVL_HOLD_OFF_MS_NAME,
		.max = CS40L26_DBC_TX_LVL_HOLD_OFF_MS_MAX,
	},
};

int cs40l26_pm_timeout_ms_set(struct cs40l26_private *cs40l26, unsigned int dsp_state,
		u32 timeout_ms)
{
	u32 reg, timeout_ticks;
	unsigned int min;
	int error;

	if (cs40l26->fw_loaded) {
		error = cl_dsp_get_reg(cs40l26->dsp, "PM_TIMER_TIMEOUT_TICKS",
				CL_DSP_XM_UNPACKED_TYPE, CS40L26_PM_ALGO_ID, &reg);
		if (error)
			return error;
	} else {
		reg = cs40l26->rom_regs->pm_timeout_ticks;
	}

	if (dsp_state == CS40L26_DSP_STATE_STANDBY) {
		reg += CS40L26_PM_STDBY_TIMEOUT_OFFSET;
		min = CS40L26_PM_STDBY_TIMEOUT_MS_MIN;
	} else if (dsp_state == CS40L26_DSP_STATE_ACTIVE) {
		reg += CS40L26_PM_ACTIVE_TIMEOUT_OFFSET;
		min = CS40L26_PM_ACTIVE_TIMEOUT_MS_MIN;
	} else {
		dev_err(cs40l26->dev, "Invalid DSP state: %u\n", dsp_state);
		return -EINVAL;
	}

	if (timeout_ms > CS40L26_PM_TIMEOUT_MS_MAX)
		timeout_ticks = (CS40L26_PM_TIMEOUT_MS_MAX * CS40L26_PM_TICKS_PER_SEC) / 1000;
	else if (timeout_ms < min)
		timeout_ticks = (min * CS40L26_PM_TICKS_PER_SEC) / 1000;
	else
		timeout_ticks = (timeout_ms * CS40L26_PM_TICKS_PER_SEC) / 1000;

	error = regmap_write(cs40l26->regmap, reg, timeout_ticks);
	if (error)
		dev_err(cs40l26->dev, "Failed to set PM timeout: %d\n", error);

	return error;
}
EXPORT_SYMBOL_GPL(cs40l26_pm_timeout_ms_set);

int cs40l26_pm_timeout_ms_get(struct cs40l26_private *cs40l26, unsigned int dsp_state,
		u32 *timeout_ms)
{
	u32 reg, timeout_ticks;
	int error;

	if (cs40l26->fw_loaded) {
		error = cl_dsp_get_reg(cs40l26->dsp, "PM_TIMER_TIMEOUT_TICKS",
				CL_DSP_XM_UNPACKED_TYPE, CS40L26_PM_ALGO_ID, &reg);
		if (error)
			return error;
	} else {
		reg = cs40l26->rom_regs->pm_timeout_ticks;
	}

	if (dsp_state == CS40L26_DSP_STATE_STANDBY) {
		reg += CS40L26_PM_STDBY_TIMEOUT_OFFSET;
	} else if (dsp_state == CS40L26_DSP_STATE_ACTIVE) {
		reg += CS40L26_PM_ACTIVE_TIMEOUT_OFFSET;
	} else {
		dev_err(cs40l26->dev, "Invalid DSP state: %u\n", dsp_state);
		return -EINVAL;
	}

	error = regmap_read(cs40l26->regmap, reg, &timeout_ticks);
	if (error) {
		dev_err(cs40l26->dev, "Failed to get PM timeout: %d\n", error);
		return error;
	}

	*timeout_ms = DIV_ROUND_UP(timeout_ticks * 1000, CS40L26_PM_TICKS_PER_SEC);

	return 0;
}
EXPORT_SYMBOL_GPL(cs40l26_pm_timeout_ms_get);

inline void cs40l26_pm_runtime_setup(struct cs40l26_private *cs40l26)
{
	pm_runtime_mark_last_busy(cs40l26->dev);
	pm_runtime_use_autosuspend(cs40l26->dev);
	pm_runtime_set_autosuspend_delay(cs40l26->dev, CS40L26_AUTOSUSPEND_DELAY_MS);
	pm_runtime_enable(cs40l26->dev);
}
EXPORT_SYMBOL_GPL(cs40l26_pm_runtime_setup);

inline void cs40l26_pm_runtime_teardown(struct cs40l26_private *cs40l26)
{
	pm_runtime_dont_use_autosuspend(cs40l26->dev);
	pm_runtime_disable(cs40l26->dev);
}
EXPORT_SYMBOL_GPL(cs40l26_pm_runtime_teardown);

static int cs40l26_check_pm_lock(struct cs40l26_private *cs40l26, bool *locked)
{
	unsigned int dsp_lock, reg;
	int error;

	if (cs40l26->fw_loaded) {
		error = cl_dsp_get_reg(cs40l26->dsp, "PM_STATE_LOCKS", CL_DSP_XM_UNPACKED_TYPE,
				CS40L26_PM_ALGO_ID, &reg);
		if (error)
			return error;
	} else {
		reg = cs40l26->rom_regs->pm_state_locks;
	}

	error = regmap_read(cs40l26->regmap, reg + CS40L26_DSP_LOCK3_OFFSET, &dsp_lock);
	if (error)
		return error;

	if (dsp_lock & CS40L26_DSP_LOCK3_MASK)
		*locked = true;
	else
		*locked = false;

	return 0;
}

static void cs40l26_remove_asp_scaling(struct cs40l26_private *cs40l26)
{
	struct device *dev = cs40l26->dev;
	u16 gain;

	if (cs40l26->asp_scale_pct >= CS40L26_GAIN_FULL_SCALE || !cs40l26->scaling_applied)
		return;

	gain = cs40l26->gain_tmp;

	if (gain >= CS40L26_NUM_PCT_MAP_VALUES) {
		dev_info(dev, "Gain %u%% out of bounds\n", gain);
		return;
	}

	cs40l26->gain_pct = gain;
	cs40l26->scaling_applied = false;

	queue_work(cs40l26->vibe_workqueue, &cs40l26->set_gain_work);
}

int cs40l26_pm_state_transition(struct cs40l26_private *cs40l26, enum cs40l26_pm_state state)
{
	struct device *dev = cs40l26->dev;
	u32 cmd, he_time_cmd, he_time_cmd_payload;
	u8 curr_state;
	bool dsp_lock;
	int error, i;

	dev_dbg(cs40l26->dev, "%s: to state = %d\n", __func__, state);
	
	cmd = (u32) CS40L26_DSP_MBOX_PM_CMD_BASE + state;

	switch (state) {
	case CS40L26_PM_STATE_WAKEUP:
		error = cs40l26_mailbox_write(cs40l26, cmd);
		if (error)
			return error;

		break;
	case CS40L26_PM_STATE_PREVENT_HIBERNATE:
		for (i = 0; i < CS40L26_DSP_STATE_ATTEMPTS; i++) {
			error = cs40l26_mailbox_write(cs40l26, cmd);
			if (error)
				return error;

			error = cs40l26_dsp_state_get(cs40l26, &curr_state);
			if (error)
				return error;

			if (curr_state == CS40L26_DSP_STATE_ACTIVE)
				break;

			if (curr_state == CS40L26_DSP_STATE_STANDBY) {
				error = cs40l26_check_pm_lock(cs40l26, &dsp_lock);
				if (error)
					return error;

				if (dsp_lock)
					break;
			}
			usleep_range(5000, 5100);
		}

		if (i == CS40L26_DSP_STATE_ATTEMPTS) {
			dev_err(cs40l26->dev, "DSP not starting\n");
			return -ETIMEDOUT;
		}

		if (cs40l26->allow_hibernate_sent) {
			/*
			 * send time elapsed since last ALLOW_HIBERNATE mailbox
			 * command to provide input to thermal model
			 */
			if (timer_pending(&cs40l26->hibernate_timer)) {
				he_time_cmd_payload = ktime_to_ms(ktime_sub(
						ktime_get_boottime(),
						cs40l26->allow_hibernate_ts));
				if (he_time_cmd_payload > CS40L26_DSP_MBOX_HE_PAYLOAD_MAX_MS)
					he_time_cmd_payload = CS40L26_DSP_MBOX_HE_PAYLOAD_OVERFLOW;
			} else {
				he_time_cmd_payload =
					CS40L26_DSP_MBOX_HE_PAYLOAD_OVERFLOW;
			}

			dev_dbg(dev, "HE_TIME payload, 0x%06X",
							he_time_cmd_payload);

			he_time_cmd = CS40L26_DSP_MBOX_CMD_HE_TIME_BASE |
					he_time_cmd_payload;

			error = cs40l26_dsp_write(cs40l26,
						CS40L26_DSP_VIRTUAL1_MBOX_1,
						he_time_cmd);
			if (error)
				return error;
		}

		break;
	case CS40L26_PM_STATE_ALLOW_HIBERNATE:
		cs40l26->wksrc_sts = 0x00;
		error = cs40l26_dsp_write(cs40l26, CS40L26_DSP_VIRTUAL1_MBOX_1, cmd);
		if (error)
			return error;

		cs40l26->allow_hibernate_sent = true;

		mod_timer(&cs40l26->hibernate_timer, jiffies +
			msecs_to_jiffies(CS40L26_DSP_MBOX_HE_PAYLOAD_MAX_MS));

		cs40l26->allow_hibernate_ts = ktime_get_boottime();

		break;
	case CS40L26_PM_STATE_SHUTDOWN:
		cs40l26->wksrc_sts = 0x00;
		error = cs40l26_mailbox_write(cs40l26, cmd);
		if (error)
			return error;

		break;
	default:
		dev_err(dev, "Invalid PM state: %u\n", state);
		return -EINVAL;
	}

	cs40l26->pm_state = state;
	
	dev_dbg(cs40l26->dev, "%s: done to state = %d\n", __func__, cs40l26->pm_state);

	return 0;
}

static int cs40l26_dsp_start(struct cs40l26_private *cs40l26)
{
	u8 dsp_state;
	int error;

	error = regmap_write(cs40l26->regmap, CS40L26_DSP1_CCM_CORE_CONTROL,
			CS40L26_DSP_CCM_CORE_RESET);
	if (error) {
		dev_err(cs40l26->dev, "Failed to reset DSP core\n");
		return error;
	}

	error = cs40l26_dsp_state_get(cs40l26, &dsp_state);
	if (error)
		return error;

	if (dsp_state != CS40L26_DSP_STATE_ACTIVE && dsp_state != CS40L26_DSP_STATE_STANDBY) {
		dev_err(cs40l26->dev, "Failed to wake DSP core\n");
		return -EINVAL;
	}

	return 0;
}

static int cs40l26_dsp_pre_config(struct cs40l26_private *cs40l26)
{
	u32 halo_state, timeout_ms;
	u8 dsp_state;
	int error, i;

	error = cs40l26_pm_state_transition(cs40l26, CS40L26_PM_STATE_PREVENT_HIBERNATE);
	if (error)
		return error;

	error = regmap_read(cs40l26->regmap, cs40l26->rom_regs->dsp_halo_state, &halo_state);
	if (error) {
		dev_err(cs40l26->dev, "Failed to get HALO state\n");
		return error;
	}

	if (halo_state != CS40L26_DSP_HALO_STATE_RUN) {
		dev_err(cs40l26->dev, "DSP not Ready: HALO_STATE: %08X\n", halo_state);
		return -EINVAL;
	}

	error = cs40l26_pm_timeout_ms_get(cs40l26, CS40L26_DSP_STATE_ACTIVE, &timeout_ms);
	if (error)
		return error;

	for (i = 0; i < CS40L26_DSP_SHUTDOWN_MAX_ATTEMPTS; i++) {
		error = cs40l26_dsp_state_get(cs40l26, &dsp_state);
		if (error)
			return error;

		if (dsp_state != CS40L26_DSP_STATE_SHUTDOWN &&
				dsp_state != CS40L26_DSP_STATE_STANDBY)
			dev_warn(cs40l26->dev, "DSP core not safe to kill\n");
		else
			break;

		usleep_range(CS40L26_MS_TO_US(timeout_ms), CS40L26_MS_TO_US(timeout_ms) + 100);
	}

	if (i == CS40L26_DSP_SHUTDOWN_MAX_ATTEMPTS) {
		dev_err(cs40l26->dev, "DSP Core could not be shut down\n");
		return -EINVAL;
	}

	error = regmap_write(cs40l26->regmap, CS40L26_DSP1_CCM_CORE_CONTROL,
			CS40L26_DSP_CCM_CORE_KILL);
	if (error)
		dev_err(cs40l26->dev, "Failed to kill DSP core\n");

	return error;
}

static int cs40l26_mbox_buffer_read(struct cs40l26_private *cs40l26, u32 *val)
{
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	u32 base, last, len,  mbox_response, read_ptr, reg, status, write_ptr;
	u32 buffer[CS40L26_DSP_MBOX_BUFFER_NUM_REGS];
	int error;

	error = cl_dsp_get_reg(cs40l26->dsp, "QUEUE_BASE", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_MAILBOX_ALGO_ID, &reg);
	if (error)
		return error;

	error = regmap_bulk_read(regmap, reg, buffer, CS40L26_DSP_MBOX_BUFFER_NUM_REGS);
	if (error) {
		dev_err(dev, "Failed to read buffer contents\n");
		return error;
	}

	base = buffer[0];
	len = buffer[1];
	write_ptr = buffer[2];
	read_ptr = buffer[3];
	last = base + ((len - 1) * CL_DSP_BYTES_PER_WORD);

	error = cl_dsp_get_reg(cs40l26->dsp, "STATUS", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_MAILBOX_ALGO_ID, &reg);
	if (error)
		return error;

	error = regmap_read(regmap, reg, &status);
	if (error) {
		dev_err(dev, "Failed to read mailbox status\n");
		return error;
	}

	if (status) {
		dev_err(dev, "Mailbox status error: 0x%X\n", status);
		return -ENOSPC;
	}

	if (read_ptr == write_ptr) {
		dev_dbg(dev, "Reached end of queue\n");
		return 1;
	}

	error = regmap_read(regmap, read_ptr, &mbox_response);
	if (error) {
		dev_err(dev, "Failed to read from mailbox buffer\n");
		return error;
	}

	if (read_ptr == last)
		read_ptr = base;
	else
		read_ptr += CL_DSP_BYTES_PER_WORD;

	error = cl_dsp_get_reg(cs40l26->dsp, "QUEUE_RD", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_MAILBOX_ALGO_ID, &reg);
	if (error)
		return error;

	error = regmap_write(regmap, reg, read_ptr);
	if (error) {
		dev_err(dev, "Failed to update read pointer\n");
		return error;
	}

	*val = mbox_response;

	return 0;
}

static irqreturn_t cs40l26_handle_mbox_buffer(int irq, void *data)
{
	struct cs40l26_private *cs40l26 = data;
	irqreturn_t irq_status = IRQ_HANDLED;
	struct device *dev = cs40l26->dev;
	u32 val = 0;
	int error;

	mutex_lock(&cs40l26->lock);

	while (!cs40l26_mbox_buffer_read(cs40l26, &val)) {
		if ((val & CS40L26_DSP_MBOX_CMD_INDEX_MASK) == CS40L26_DSP_MBOX_PANIC) {
			dev_alert(dev, "DSP PANIC! Error condition: 0x%06X\n",
			(u32) (val & CS40L26_DSP_MBOX_CMD_PAYLOAD_MASK));
			irq_status = IRQ_HANDLED;
			goto err_mutex;
		}

		if ((val & CS40L26_DSP_MBOX_CMD_INDEX_MASK) == CS40L26_DSP_MBOX_WATERMARK) {
			dev_dbg(dev, "Mailbox: WATERMARK\n");
#ifdef CONFIG_DEBUG_FS
			error = cl_dsp_logger_update(cs40l26->cl_dsp_db);
			if (error) {
				irq_status = IRQ_NONE;
				goto err_mutex;
			}
#endif
			continue;
		}

		switch (val) {
		case CS40L26_DSP_MBOX_COMPLETE_MBOX:
			dev_dbg(dev, "Mailbox: COMPLETE_MBOX\n");
			complete_all(&cs40l26->erase_cont);
			cs40l26_vibe_state_update(cs40l26, CS40L26_VIBE_STATE_EVENT_MBOX_COMPLETE);
			break;
		case CS40L26_DSP_MBOX_COMPLETE_GPIO:
			dev_dbg(dev, "Mailbox: COMPLETE_GPIO\n");
			cs40l26_vibe_state_update(cs40l26, CS40L26_VIBE_STATE_EVENT_GPIO_COMPLETE);
			break;
		case CS40L26_DSP_MBOX_COMPLETE_I2S:
			dev_dbg(dev, "Mailbox: COMPLETE_I2S\n");
			/* ASP is interrupted */
			if (cs40l26->asp_enable)
				complete(&cs40l26->i2s_cont);
			break;
		case CS40L26_DSP_MBOX_TRIGGER_I2S:
			dev_dbg(dev, "Mailbox: TRIGGER_I2S\n");
			complete(&cs40l26->i2s_cont);
			break;
		case CS40L26_DSP_MBOX_TRIGGER_CP:
			if (!cs40l26->vibe_state_reporting) {
				dev_err(dev, "vibe_state not supported\n");
				irq_status = IRQ_HANDLED;
				goto err_mutex;
			}

			dev_dbg(dev, "Mailbox: TRIGGER_CP\n");
			cs40l26_vibe_state_update(cs40l26, CS40L26_VIBE_STATE_EVENT_MBOX_PLAYBACK);
			break;
		case CS40L26_DSP_MBOX_TRIGGER_GPIO:
			dev_dbg(dev, "Mailbox: TRIGGER_GPIO\n");
			cs40l26_vibe_state_update(cs40l26, CS40L26_VIBE_STATE_EVENT_GPIO_TRIGGER);
			break;
		case CS40L26_DSP_MBOX_PM_AWAKE:
			cs40l26->wksrc_sts |= CS40L26_WKSRC_STS_EN;
			dev_dbg(dev, "Mailbox: AWAKE\n");
			break;
		case CS40L26_DSP_MBOX_INIT:
			dev_dbg(dev, "Mailbox: INIT\n");
			break;
		case CS40L26_DSP_MBOX_F0_EST_START:
			dev_dbg(dev, "Mailbox: F0_EST_START\n");
			break;
		case CS40L26_DSP_MBOX_F0_EST_DONE:
			dev_dbg(dev, "Mailbox: F0_EST_DONE\n");
			complete(&cs40l26->cal_f0_cont);
			break;
		case CS40L26_DSP_MBOX_REDC_EST_START:
			dev_dbg(dev, "Mailbox: REDC_EST_START\n");
			break;
		case CS40L26_DSP_MBOX_REDC_EST_DONE:
			dev_dbg(dev, "Mailbox: REDC_EST_DONE\n");
			complete(&cs40l26->cal_redc_cont);
			break;
		case CS40L26_DSP_MBOX_LS_CALIBRATION_START:
			dev_dbg(dev, "Mailbox: LS_CALIBRATION_START\n");
			break;
		case CS40L26_DSP_MBOX_LS_CALIBRATION_DONE:
			dev_dbg(dev, "Mailbox: LS_CALIBRATION_DONE\n");
			complete(&cs40l26->cal_ls_cont);
			break;
		case CS40L26_DSP_MBOX_LS_CALIBRATION_ERROR:
			dev_warn(dev, "Mailbox: LS_CALIBRATION_ERROR\n");
			complete(&cs40l26->cal_ls_cont);
			break;
		case CS40L26_DSP_MBOX_LE_EST_START:
			dev_dbg(dev, "Mailbox: LE_EST_START\n");
			break;
		case CS40L26_DSP_MBOX_LE_EST_DONE:
			dev_dbg(dev, "Mailbox: LE_EST_DONE\n");
			break;
		case CS40L26_DSP_MBOX_PEQ_CALCULATION_START:
			dev_dbg(dev, "Mailbox: PEQ_CALCULATION_START\n");
			break;
		case CS40L26_DSP_MBOX_PEQ_CALCULATION_DONE:
			dev_dbg(dev, "Mailbox: PEQ_CALCULATION_DONE\n");
			complete(&cs40l26->cal_dvl_peq_cont);
			break;
		case CS40L26_DSP_MBOX_SYS_ACK:
			dev_err(dev, "Mailbox: ACK\n");
			irq_status = IRQ_HANDLED;
			goto err_mutex;
		default:
			dev_err(dev, "MBOX buffer value (0x%X) is invalid\n", val);
			irq_status = IRQ_HANDLED;
			goto err_mutex;
		}
	}

err_mutex:
	mutex_unlock(&cs40l26->lock);

	return irq_status;
}

int cs40l26_copy_f0_est_to_dvl(struct cs40l26_private *cs40l26)
{
	u32 reg, f0_measured_q9_14, global_sample_rate, normalized_f0_q1_23;
	int error, sample_rate;

	/* Must be awake and under mutex lock */
	error = regmap_read(cs40l26->regmap, CS40L26_GLOBAL_SAMPLE_RATE, &global_sample_rate);
	if (error)
		return error;

	switch (global_sample_rate & CS40L26_GLOBAL_FS_MASK) {
	case CS40L26_GLOBAL_FS_48K:
		sample_rate = 48000;
		break;
	case CS40L26_GLOBAL_FS_96K:
		sample_rate = 96000;
		break;
	default:
		dev_warn(cs40l26->dev, "Invalid GLOBAL_FS, %08X", global_sample_rate);
		return -EINVAL;
	}

	error = cl_dsp_get_reg(cs40l26->dsp, "F0_EST", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_F0_EST_ALGO_ID, &reg);
	if (error)
		return error;

	error = regmap_read(cs40l26->regmap, reg, &f0_measured_q9_14);
	if (error)
		return error;

	error = cl_dsp_get_reg(cs40l26->dsp, "LRA_NORM_F0", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_DVL_ALGO_ID, &reg);
	if (error)
		return error;

	normalized_f0_q1_23 = (f0_measured_q9_14 << 9) / sample_rate;

	return regmap_write(cs40l26->regmap, reg, normalized_f0_q1_23);
}
EXPORT_SYMBOL_GPL(cs40l26_copy_f0_est_to_dvl);

int cs40l26_asp_start(struct cs40l26_private *cs40l26)
{
	int error;

	if (cs40l26->revid != CS40L26_REVID_B2 && cs40l26->asp_scale_pct < CS40L26_GAIN_FULL_SCALE)
		queue_work(cs40l26->vibe_workqueue, &cs40l26->set_gain_work);

	error = cs40l26_mailbox_write(cs40l26, CS40L26_STOP_PLAYBACK);
	if (error) {
		dev_err(cs40l26->dev, "Failed to stop playback before I2S start\n");
		return error;
	}

	reinit_completion(&cs40l26->i2s_cont);

	/* Ensure PLL config is not already saved */
	if (!cs40l26->refclk_input) {
		error = regmap_read(cs40l26->regmap, CS40L26_REFCLK_INPUT, &cs40l26->refclk_input);
		if (error)
			return error;
	}

	return cs40l26_mailbox_write(cs40l26, CS40L26_DSP_MBOX_CMD_START_I2S);
}
EXPORT_SYMBOL_GPL(cs40l26_asp_start);

void cs40l26_vibe_state_update(struct cs40l26_private *cs40l26, enum cs40l26_vibe_state_event event)
{
	if (!mutex_is_locked(&cs40l26->lock)) {
		dev_err(cs40l26->dev, "%s must be called under mutex lock\n", __func__);
		return;
	}

	dev_dbg(cs40l26->dev, "effects_in_flight = %d, event = %d\n", cs40l26->effects_in_flight, event);

	switch (event) {
	case CS40L26_VIBE_STATE_EVENT_MBOX_PLAYBACK:
	case CS40L26_VIBE_STATE_EVENT_GPIO_TRIGGER:
		if (cs40l26->revid != CS40L26_REVID_B2)
			cs40l26_remove_asp_scaling(cs40l26);
		cs40l26->effects_in_flight = cs40l26->effects_in_flight <= 0 ? 1 :
			cs40l26->effects_in_flight + 1;
		break;
	case CS40L26_VIBE_STATE_EVENT_MBOX_COMPLETE:
	case CS40L26_VIBE_STATE_EVENT_GPIO_COMPLETE:
		cs40l26->effects_in_flight = cs40l26->effects_in_flight <= 0 ? 0 :
			cs40l26->effects_in_flight - 1;
		if (cs40l26->effects_in_flight == 0 && cs40l26->asp_enable)
			if (cs40l26_asp_start(cs40l26))
				return;
		break;
	case CS40L26_VIBE_STATE_EVENT_ASP_START:
		cs40l26->asp_enable = true;
		break;
	case CS40L26_VIBE_STATE_EVENT_ASP_STOP:
		if (cs40l26->revid != CS40L26_REVID_B2)
			cs40l26_remove_asp_scaling(cs40l26);
		
		/* Restore PLL_OPEN_LOOP configuration */
		if (cs40l26_set_pll_loop(cs40l26, (cs40l26->refclk_input &
						CS40L26_PLL_REFCLK_LOOP_MASK)>>CS40L26_PLL_REFCLK_LOOP_SHIFT))
				return;
		
		cs40l26->refclk_input = 0;

		cs40l26->asp_enable = false;
		break;
	default:
		dev_err(cs40l26->dev, "Invalid vibe state event: %d\n", event);
		break;
	}

	if (cs40l26->effects_in_flight)
		cs40l26->vibe_state = CS40L26_VIBE_STATE_HAPTIC;
	else if (cs40l26->asp_enable)
		cs40l26->vibe_state = CS40L26_VIBE_STATE_ASP;
	else
		cs40l26->vibe_state = CS40L26_VIBE_STATE_STOPPED;

	sysfs_notify(&cs40l26->dev->kobj, NULL, "vibe_state");
}
EXPORT_SYMBOL_GPL(cs40l26_vibe_state_update);

static int cs40l26_error_release(struct cs40l26_private *cs40l26,
		unsigned int err_rls)
{
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	u32 err_sts, err_cfg;
	int error;

	error = regmap_read(regmap, CS40L26_ERROR_RELEASE, &err_sts);
	if (error) {
		dev_err(cs40l26->dev, "Failed to get error status\n");
		return error;
	}

	err_cfg = err_sts & ~BIT(err_rls);

	error = regmap_write(cs40l26->regmap, CS40L26_ERROR_RELEASE, err_cfg);
	if (error) {
		dev_err(dev, "Actuator Safe Mode release sequence failed\n");
		return error;
	}

	err_cfg |= BIT(err_rls);

	error = regmap_write(regmap, CS40L26_ERROR_RELEASE, err_cfg);
	if (error) {
		dev_err(dev, "Actuator Safe Mode release sequence failed\n");
		return error;
	}

	err_cfg &= ~BIT(err_rls);

	error = regmap_write(cs40l26->regmap, CS40L26_ERROR_RELEASE, err_cfg);
	if (error)
		dev_err(dev, "Actuator Safe Mode release sequence failed\n");

	return error;
}

static int cs40l26_handle_pre_irq(void *irq_drv_data)
{
	struct cs40l26_private *cs40l26 = irq_drv_data;
	unsigned int sts;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	error = regmap_read(cs40l26->regmap, CS40L26_IRQ1_STATUS, &sts);
	if (error)
		goto err_pm;

	if (!(sts & CS40L26_IRQ_STATUS_MASK))
		dev_err(cs40l26->dev, "IRQ1 asserted with no pending interrupts\n");

err_pm:
	cs40l26_pm_exit(cs40l26->dev);

	return error;
}

static irqreturn_t cs40l26_gpio_rise(int irq, void *data)
{
	struct cs40l26_private *cs40l26 = data;

	mutex_lock(&cs40l26->lock);

	if (cs40l26->wksrc_sts & CS40L26_WKSRC_STS_EN)
		dev_dbg(cs40l26->dev, "GPIO rising edge detected\n");

	cs40l26->wksrc_sts |= CS40L26_WKSRC_STS_EN;

	mutex_unlock(&cs40l26->lock);

	return IRQ_HANDLED;
}

static irqreturn_t cs40l26_gpio_fall(int irq, void *data)
{
	struct cs40l26_private *cs40l26 = data;

	mutex_lock(&cs40l26->lock);

	if (cs40l26->wksrc_sts & CS40L26_WKSRC_STS_EN)
		dev_dbg(cs40l26->dev, "GPIO falling edge detected\n");

	cs40l26->wksrc_sts |= CS40L26_WKSRC_STS_EN;

	mutex_unlock(&cs40l26->lock);

	return IRQ_HANDLED;
}

static irqreturn_t cs40l26_wakesource_any(int irq, void *data)
{
	struct cs40l26_private *cs40l26 = data;
	int error;
	u32 val, reg;

	dev_dbg(cs40l26->dev, "Wakesource detected (ANY)\n");

	error = regmap_read(cs40l26->regmap, CS40L26_PWRMGT_STS, &val);
	if (error) {
		dev_err(cs40l26->dev, "Failed to get Power Management Status\n");
		return IRQ_NONE;
	}

	mutex_lock(&cs40l26->lock);

	cs40l26->wksrc_sts = (u8) ((val & CS40L26_WKSRC_STS_MASK) >>
				CS40L26_WKSRC_STS_SHIFT);

	error = cl_dsp_get_reg(cs40l26->dsp, "LAST_WAKESRC_CTL",
			CL_DSP_XM_UNPACKED_TYPE, cs40l26->fw_id, &reg);
	if (error)
		return IRQ_NONE;

	error = regmap_read(cs40l26->regmap, reg, &val);
	if (error) {
		dev_err(cs40l26->dev, "Failed to read LAST_WAKESRC_CTL\n");
		return IRQ_NONE;
	}

	cs40l26->last_wksrc_pol = (u8) (val & CS40L26_WKSRC_GPIO_POL_MASK);

	mutex_unlock(&cs40l26->lock);

	return IRQ_HANDLED;
}

static irqreturn_t cs40l26_wakesource_gpio(int irq, void *data)
{
	struct cs40l26_private *cs40l26 = data;

	dev_dbg(cs40l26->dev, "GPIO event woke device from hibernate\n");

	mutex_lock(&cs40l26->lock);

	if (cs40l26->wksrc_sts & cs40l26->last_wksrc_pol) {
		dev_dbg(cs40l26->dev, "GPIO falling edge detected\n");
		cs40l26->wksrc_sts |= CS40L26_WKSRC_STS_EN;
	} else {
		dev_dbg(cs40l26->dev, "GPIO rising edge detected\n");
	}

	mutex_unlock(&cs40l26->lock);

	return IRQ_HANDLED;
}

static irqreturn_t cs40l26_wakesource_iic(int irq, void *data)
{
	struct cs40l26_private *cs40l26 = data;

	dev_dbg(cs40l26->dev, "I2C event woke device from hibernate\n");

	return IRQ_HANDLED;
}

static irqreturn_t cs40l26_bst_ovp_err(int irq, void *data)
{
	struct cs40l26_private *cs40l26 = data;

	dev_err(cs40l26->dev, "BST overvolt. error\n");

	return IRQ_RETVAL(!cs40l26_error_release(cs40l26, CS40L26_BST_OVP_ERR_RLS));
}

static irqreturn_t cs40l26_bst_uv_err(int irq, void *data)
{
	struct cs40l26_private *cs40l26 = data;

	dev_err(cs40l26->dev, "BST undervolt. error\n");

	return IRQ_RETVAL(!cs40l26_error_release(cs40l26, CS40L26_BST_UVP_ERR_RLS));
}

static irqreturn_t cs40l26_bst_short(int irq, void *data)
{
	struct cs40l26_private *cs40l26 = data;

	dev_err(cs40l26->dev, "LBST short detected\n");

	return IRQ_RETVAL(!cs40l26_error_release(cs40l26, CS40L26_BST_SHORT_ERR_RLS));
}

static irqreturn_t cs40l26_ipk_flag(int irq, void *data)
{
	struct cs40l26_private *cs40l26 = data;

	dev_dbg(cs40l26->dev, "Current is being limited by LBST inductor\n");

	return IRQ_HANDLED;
}

static irqreturn_t cs40l26_temp_err(int irq, void *data)
{
	struct cs40l26_private *cs40l26 = data;

	dev_err(cs40l26->dev, "Die overtemperature error\n");

	return IRQ_RETVAL(!cs40l26_error_release(cs40l26, CS40L26_TEMP_ERR_RLS));
}

static irqreturn_t cs40l26_amp_short(int irq, void *data)
{
	struct cs40l26_private *cs40l26 = data;

	dev_err(cs40l26->dev, "AMP short detected\n");

	return IRQ_RETVAL(!cs40l26_error_release(cs40l26, CS40L26_AMP_SHORT_ERR_RLS));
}

static irqreturn_t cs40l26_watchdog_rise(int irq, void *data)
{
	struct cs40l26_private *cs40l26 = data;

	dev_warn(cs40l26->dev, "Watchdog: A DC level has been detected\n");

	return IRQ_HANDLED;
}

static irqreturn_t cs40l26_watchdog_fall(int irq, void *data)
{
	struct cs40l26_private *cs40l26 = data;

	dev_info(cs40l26->dev, "Watchdog: The previously-detected DC level has been removed\n");

	return IRQ_HANDLED;
}

static irqreturn_t cs40l26_vpbr_flag(int irq, void *data)
{
	//struct cs40l26_private *cs40l26 = data;

	//dev_err(cs40l26->dev, "VP voltage has dropped below brownout threshold\n");

	return IRQ_HANDLED;
}

static irqreturn_t cs40l26_vpbr_att_clr(int irq, void *data)
{
	struct cs40l26_private *cs40l26 = data;

	dev_warn(cs40l26->dev, "Cleared attenuation applied by VP brownout event\n");

	return IRQ_HANDLED;
}

static irqreturn_t cs40l26_vbbr_flag(int irq, void *data)
{
	//struct cs40l26_private *cs40l26 = data;

	//dev_err(cs40l26->dev, "VBST voltage has dropped below brownout threshold\n");

	return IRQ_HANDLED;
}

static irqreturn_t cs40l26_vbst_att_clr(int irq, void *data)
{
	struct cs40l26_private *cs40l26 = data;

	dev_dbg(cs40l26->dev, "Cleared attenuation caused by VBST brownout\n");

	return IRQ_HANDLED;
}

static const struct cs40l26_irq cs40l26_irqs[] = {
	CS40L26_IRQ(GPIO1_RISE, "GPIO1 rise", cs40l26_gpio_rise),
	CS40L26_IRQ(GPIO1_FALL, "GPIO1 fall", cs40l26_gpio_fall),
	CS40L26_IRQ(GPIO2_RISE, "GPIO2 rise", cs40l26_gpio_rise),
	CS40L26_IRQ(GPIO2_FALL, "GPIO2 fall", cs40l26_gpio_fall),
	CS40L26_IRQ(GPIO3_RISE, "GPIO3 rise", cs40l26_gpio_rise),
	CS40L26_IRQ(GPIO3_FALL, "GPIO3 fall", cs40l26_gpio_fall),
	CS40L26_IRQ(GPIO4_RISE, "GPIO4 rise", cs40l26_gpio_rise),
	CS40L26_IRQ(GPIO4_FALL, "GPIO4 fall", cs40l26_gpio_fall),
	CS40L26_IRQ(WKSRC_STS_ANY, "Wakesource any", cs40l26_wakesource_any),
	CS40L26_IRQ(WKSRC_STS_GPIO1, "Wakesource GPIO1", cs40l26_wakesource_gpio),
	CS40L26_IRQ(WKSRC_STS_GPIO2, "Wakesource GPIO2", cs40l26_wakesource_gpio),
	CS40L26_IRQ(WKSRC_STS_GPIO3, "Wakesource GPIO3", cs40l26_wakesource_gpio),
	CS40L26_IRQ(WKSRC_STS_GPIO4, "Wakesource GPIO4", cs40l26_wakesource_gpio),
	CS40L26_IRQ(WKSRC_STS_I2C, "Wakesource I2C", cs40l26_wakesource_iic),
	CS40L26_IRQ(BST_OVP_ERR, "Boost overvoltage error", cs40l26_bst_ovp_err),
	CS40L26_IRQ(BST_DCM_UVP_ERR, "Boost undervoltage error", cs40l26_bst_uv_err),
	CS40L26_IRQ(BST_SHORT_ERR, "Boost short", cs40l26_bst_short),
	CS40L26_IRQ(BST_IPK_FLAG, "Current limited", cs40l26_ipk_flag),
	CS40L26_IRQ(TEMP_ERR, "Die overtemperature error", cs40l26_temp_err),
	CS40L26_IRQ(AMP_ERR, "Amp short", cs40l26_amp_short),
	CS40L26_IRQ(DC_WD_RISE, "DC watchdog triggered", cs40l26_watchdog_rise),
	CS40L26_IRQ(DC_WD_FALL, "DC watchdog cleared", cs40l26_watchdog_fall),
	CS40L26_IRQ(VIRTUAL2_MBOX_WR, "Mailbox interrupt", cs40l26_handle_mbox_buffer),
	CS40L26_IRQ(VPBR_FLAG, "VP brownout", cs40l26_vpbr_flag),
	CS40L26_IRQ(VPBR_ATT_CLR, "VPBR attenuation cleared", cs40l26_vpbr_att_clr),
	CS40L26_IRQ(VBBR_FLAG, "VBST brownout", cs40l26_vbbr_flag),
	CS40L26_IRQ(VBBR_ATT_CLR, "VBST attenuation cleared", cs40l26_vbst_att_clr),
};

static const struct regmap_irq cs40l26_reg_irqs[] = {
	CS40L26_REG_IRQ(IRQ1_EINT_1, GPIO1_RISE),
	CS40L26_REG_IRQ(IRQ1_EINT_1, GPIO1_FALL),
	CS40L26_REG_IRQ(IRQ1_EINT_1, GPIO2_RISE),
	CS40L26_REG_IRQ(IRQ1_EINT_1, GPIO2_FALL),
	CS40L26_REG_IRQ(IRQ1_EINT_1, GPIO3_RISE),
	CS40L26_REG_IRQ(IRQ1_EINT_1, GPIO3_FALL),
	CS40L26_REG_IRQ(IRQ1_EINT_1, GPIO4_RISE),
	CS40L26_REG_IRQ(IRQ1_EINT_1, GPIO4_FALL),
	CS40L26_REG_IRQ(IRQ1_EINT_1, WKSRC_STS_ANY),
	CS40L26_REG_IRQ(IRQ1_EINT_1, WKSRC_STS_GPIO1),
	CS40L26_REG_IRQ(IRQ1_EINT_1, WKSRC_STS_GPIO2),
	CS40L26_REG_IRQ(IRQ1_EINT_1, WKSRC_STS_GPIO3),
	CS40L26_REG_IRQ(IRQ1_EINT_1, WKSRC_STS_GPIO4),
	CS40L26_REG_IRQ(IRQ1_EINT_1, WKSRC_STS_I2C),
	CS40L26_REG_IRQ(IRQ1_EINT_1, BST_OVP_ERR),
	CS40L26_REG_IRQ(IRQ1_EINT_1, BST_DCM_UVP_ERR),
	CS40L26_REG_IRQ(IRQ1_EINT_1, BST_SHORT_ERR),
	CS40L26_REG_IRQ(IRQ1_EINT_1, BST_IPK_FLAG),
	CS40L26_REG_IRQ(IRQ1_EINT_1, TEMP_ERR),
	CS40L26_REG_IRQ(IRQ1_EINT_1, AMP_ERR),
	CS40L26_REG_IRQ(IRQ1_EINT_1, DC_WD_RISE),
	CS40L26_REG_IRQ(IRQ1_EINT_1, DC_WD_FALL),
	CS40L26_REG_IRQ(IRQ1_EINT_1, VIRTUAL2_MBOX_WR),
	CS40L26_REG_IRQ(IRQ1_EINT_2, VPBR_FLAG),
	CS40L26_REG_IRQ(IRQ1_EINT_2, VPBR_ATT_CLR),
	CS40L26_REG_IRQ(IRQ1_EINT_2, VBBR_FLAG),
	CS40L26_REG_IRQ(IRQ1_EINT_2, VBBR_ATT_CLR),
};

static struct regmap_irq_chip cs40l26_regmap_irq_chip = {
	.name = "cs40l26 IRQ1 Controller",
	.status_base = CS40L26_IRQ1_EINT_1,
	.mask_base = CS40L26_IRQ1_MASK_1,
	.ack_base = CS40L26_IRQ1_EINT_1,
	.num_regs = 2,
	.irqs = cs40l26_reg_irqs,
	.num_irqs = ARRAY_SIZE(cs40l26_reg_irqs),
	.handle_pre_irq = cs40l26_handle_pre_irq,
	.runtime_pm = true,
};

static struct cs40l26_wseq_op *cs40l26_wseq_find_op(u32 addr, u8 op_code,
		struct list_head *wseq_ops)
{
	struct cs40l26_wseq_op *op;

	list_for_each_entry(op, wseq_ops, list) {
		if (op->operation == op_code && op->address == addr)
			return op;
	}

	return NULL;
}

static int cs40l26_wseq_op_format(struct cs40l26_private *cs40l26, struct cs40l26_wseq *wseq,
		struct cs40l26_wseq_op *op, struct cl_dsp_memchunk *ch)
{
	int error;

	error = cl_dsp_memchunk_write(ch, 8, op->operation);
	if (error)
		return error;

	switch (op->operation) {
	case CS40L26_WSEQ_OP_WRITE_FULL:
		error = cl_dsp_memchunk_write(ch, 32, op->address);
		if (error)
			return error;

		error = cl_dsp_memchunk_write(ch, 32, op->data);
		break;
	case CS40L26_WSEQ_OP_WRITE_L16:
	case CS40L26_WSEQ_OP_WRITE_H16:
		error = cl_dsp_memchunk_write(ch, 24, op->address);
		if (error)
			return error;

		error = cl_dsp_memchunk_write(ch, 16, op->data);
		break;
	case CS40L26_WSEQ_OP_WRITE_ADDR8:
		error = cl_dsp_memchunk_write(ch, 8, op->address);
		if (error)
			return error;

		error = cl_dsp_memchunk_write(ch, 32, op->data);
		break;
	default:
		dev_err(cs40l26->dev, "Write Sequencer Op. code not supported 0x%02X\n",
				op->operation);
		return -EINVAL;
	}

	return error;
}

static int cs40l26_wseq_update(struct cs40l26_private *cs40l26, u32 data,
		struct cs40l26_wseq *wseq, struct cs40l26_wseq_op *op)
{
	u32 words[CS40L26_WSEQ_OP_MAX_WORDS];
	struct cl_dsp_memchunk ch;
	int error, i;

	op->data = data;

	ch = cl_dsp_memchunk_create(words, sizeof(words));

	error = cs40l26_wseq_op_format(cs40l26, wseq, op, &ch);
	if (error)
		return error;

	for (i = 0; i < ch.bytes / CL_DSP_BYTES_PER_WORD; i++)
		words[i] = be32_to_cpu(words[i]);

	return regmap_bulk_write(cs40l26->regmap, wseq->base_addr + op->offset,
			words, ch.bytes / CL_DSP_BYTES_PER_WORD);
}

static int cs40l26_wseq_new(struct cs40l26_private *cs40l26, u32 addr, u32 data,
		u8 op_code, struct cs40l26_wseq *wseq, struct cs40l26_wseq_op *op_end)
{
	u32 words[CS40L26_WSEQ_OP_MAX_WORDS];
	struct cs40l26_wseq_op *op_new;
	struct cl_dsp_memchunk ch;
	int error, i;

	op_new = kzalloc(sizeof(struct cs40l26_wseq_op), GFP_KERNEL);
	if (!op_new)
		return -ENOMEM;

	op_new->operation = op_code;
	op_new->address = addr;
	op_new->offset = op_end->offset;
	op_new->data = data;

	ch = cl_dsp_memchunk_create(words, sizeof(words));

	error = cs40l26_wseq_op_format(cs40l26, wseq, op_new, &ch);
	if (error)
		goto err_free;

	/* Check if there's enough space to add new operation */
	if ((wseq->size_bytes - op_end->offset) < ch.bytes) {
		dev_err(cs40l26->dev, "Not enough space to add new power on sequence op\n");
		error = -ENOSPC;
		goto err_free;
	}

	for (i = 0; i < ch.bytes / CL_DSP_BYTES_PER_WORD; i++)
		words[i] = be32_to_cpu(words[i]);

	error = regmap_bulk_write(cs40l26->regmap, wseq->base_addr + op_new->offset,
			words, ch.bytes / CL_DSP_BYTES_PER_WORD);
	if (error)
		goto err_free;

	list_add(&op_new->list, op_end->list.prev);
	wseq->num_ops++;

	op_end->offset += ch.bytes;

	error = regmap_write(cs40l26->regmap, wseq->base_addr + op_end->offset,
			CS40L26_WSEQ_OP_END_DATA);
	if (error) {
		list_del(&op_new->list);
		goto err_free;
	}

	return 0;

err_free:
	kfree(op_new);

	return error;
}

int cs40l26_wseq_write(struct cs40l26_private *cs40l26, u32 addr, u32 data,
		bool update, u8 op_code, struct cs40l26_wseq *wseq)
{
	struct cs40l26_wseq_op *op, *op_end;
	int error;

	op_end = cs40l26_wseq_find_op(CS40L26_WSEQ_OP_END_ADDR, CS40L26_WSEQ_OP_END,
			&wseq->ops);
	if (!op_end) {
		dev_err(cs40l26->dev, "Write sequencer list terminator\n");
		return -EINVAL;
	}

	op = cs40l26_wseq_find_op(addr, op_code, &wseq->ops);
	if (!op || !update) {
		error = cs40l26_wseq_new(cs40l26, addr, data, op_code, wseq, op_end);
		if (error)
			return error;
	} else {
		error = cs40l26_wseq_update(cs40l26, data, wseq, op);
		if (error)
			return error;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(cs40l26_wseq_write);

static int cs40l26_wseq_multi_write(struct cs40l26_private *cs40l26,
		const struct reg_sequence *reg_seq, int num_regs, bool update, u8 op_code,
		struct cs40l26_wseq *wseq)
{
	int error, i;

	for (i = 0; i < num_regs; i++) {
		error = cs40l26_wseq_write(cs40l26, reg_seq[i].reg,
				reg_seq[i].def, update, op_code, wseq);
		if (error)
			return error;
	}

	return 0;
}

static void cs40l26_wseq_clear(struct cs40l26_private *cs40l26, struct cs40l26_wseq *wseq)
{
	struct cs40l26_wseq_op *op_tmp;
	struct cs40l26_wseq_op *op;

	if (wseq->num_ops == 0)
		return;

	list_for_each_entry_safe(op, op_tmp, &wseq->ops, list) {
		list_del(&op->list);
		kfree(op);
	}

	wseq->num_ops = 0;
}

static int cs40l26_wseq_populate(struct cs40l26_private *cs40l26, struct cs40l26_wseq *wseq)
{
	struct cs40l26_wseq_op *op = NULL;
	struct cl_dsp *dsp = cs40l26->dsp;
	struct cl_dsp_memchunk ch;
	u8 *words;
	int error;

	words = kzalloc(wseq->size_bytes, GFP_KERNEL);
	if (!words)
		return -ENOMEM;

	error = regmap_raw_read(cs40l26->regmap, wseq->base_addr, words, wseq->size_bytes);
	if (error)
		goto err_free;

	ch = cl_dsp_memchunk_create(words, wseq->size_bytes);

	while (!cl_dsp_memchunk_end(&ch)) {
		op = kzalloc(sizeof(struct cs40l26_wseq_op), GFP_KERNEL);
		if (!op) {
			error = -ENOMEM;
			goto err_free;
		}

		op->offset = ch.bytes;
		error = cl_dsp_memchunk_read(dsp, &ch, 8, &op->operation);
		if (error)
			goto err_op_free;

		switch (op->operation) {
		case CS40L26_WSEQ_OP_END:
			op->data = CS40L26_WSEQ_OP_END_DATA;
			op->address = CS40L26_WSEQ_OP_END_ADDR;
			break;
		case CS40L26_WSEQ_OP_WRITE_ADDR8:
			error = cl_dsp_memchunk_read(dsp, &ch, 8, &op->address);
			if (error)
				goto err_op_free;

			error = cl_dsp_memchunk_read(dsp, &ch, 32, &op->data);
			if (error)
				goto err_op_free;
			break;
		case CS40L26_WSEQ_OP_WRITE_H16:
		case CS40L26_WSEQ_OP_WRITE_L16:
			error = cl_dsp_memchunk_read(dsp, &ch, 24, &op->address);
			if (error)
				goto err_op_free;

			error = cl_dsp_memchunk_read(dsp, &ch, 16, &op->data);
			if (error)
				goto err_op_free;
			break;
		case CS40L26_WSEQ_OP_WRITE_FULL:
			error = cl_dsp_memchunk_read(dsp, &ch, 32, &op->address);
			if (error)
				goto err_op_free;

			error = cl_dsp_memchunk_read(dsp, &ch, 32, &op->data);
			if (error)
				goto err_op_free;
			break;
		default:
			dev_err(cs40l26->dev, "Invalid OP code 0x%02X\n", op->operation);
			error = -EINVAL;
			goto err_op_free;
		}

		list_add_tail(&op->list, &wseq->ops);
		wseq->num_ops++;

		if (op->operation == CS40L26_WSEQ_OP_END)
			break;
	}

	if (op && op->operation != CS40L26_WSEQ_OP_END) {
		dev_err(cs40l26->dev, "Write sequence missing terminator\n");
		error = -EINVAL;
		goto err_free;
	}

err_free:
	kfree(words);

	return error;

err_op_free:
	kfree(words);
	kfree(op);

	return error;
}

static int cs40l26_wseq_init(struct cs40l26_private *cs40l26)
{
	int error;

	cs40l26->aseq.size_bytes = CS40L26_ASEQ_MAX_BYTES;
	cs40l26->aseq.size_words = CS40L26_ASEQ_MAX_WORDS;

	INIT_LIST_HEAD(&cs40l26->aseq.ops);

	error = cl_dsp_get_reg(cs40l26->dsp, "ACTIVE_SEQUENCE", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_PM_ALGO_ID, &cs40l26->aseq.base_addr);
	if (error)
		return error;

	error = cs40l26_wseq_populate(cs40l26, &cs40l26->aseq);
	if (error)
		return error;

	cs40l26->pseq.size_bytes = CS40L26_PSEQ_MAX_BYTES;
	cs40l26->pseq.size_words = CS40L26_PSEQ_MAX_WORDS;

	INIT_LIST_HEAD(&cs40l26->pseq.ops);

	error = cl_dsp_get_reg(cs40l26->dsp, "POWER_ON_SEQUENCE", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_PM_ALGO_ID, &cs40l26->pseq.base_addr);
	if (error)
		return error;

	error = cs40l26_wseq_populate(cs40l26, &cs40l26->pseq);
	if (error)
		return error;

	/* Configure noise gate source */
	error = cs40l26_wseq_write(cs40l26, CS40L26_NGATE1_INPUT, CS40L26_DATA_SRC_DSP1TX4,
			true, CS40L26_WSEQ_OP_WRITE_L16, &cs40l26->pseq);
	if (error)
		return error;

	/* Set speaker output to HI-Z when amplifier is disabled */
	return cs40l26_wseq_write(cs40l26, CS40L26_TST_DAC_MSM_CONFIG, CS40L26_SPK_DEFAULT_HIZ,
			true, CS40L26_WSEQ_OP_WRITE_H16, &cs40l26->pseq);
}

static int cs40l26_irq_update_mask(struct cs40l26_private *cs40l26, u32 reg, u32 val, u32 bit_mask)
{
	u32 eint_reg, cur_mask, new_mask;
	int error;

	if (reg == CS40L26_IRQ1_MASK_1) {
		eint_reg = CS40L26_IRQ1_EINT_1;
	} else if (reg == CS40L26_IRQ1_MASK_2) {
		eint_reg = CS40L26_IRQ1_EINT_2;
	} else {
		dev_err(cs40l26->dev, "Invalid IRQ mask reg: 0x%08X\n", reg);
		return -EINVAL;
	}

	error = regmap_read(cs40l26->regmap, reg, &cur_mask);
	if  (error) {
		dev_err(cs40l26->dev, "Failed to get IRQ mask\n");
		return error;
	}

	new_mask = (cur_mask & ~bit_mask) | val;

	/* Clear interrupt prior to masking/unmasking */
	error = regmap_write(cs40l26->regmap, eint_reg, bit_mask);
	if (error) {
		dev_err(cs40l26->dev, "Failed to clear IRQ\n");
		return error;
	}

	error = regmap_write(cs40l26->regmap, reg, new_mask);
	if (error) {
		dev_err(cs40l26->dev, "Failed to update IRQ mask\n");
		return error;
	}

	if (bit_mask & CS40L26_WSEQ_UPPER_MASK) {
		error = cs40l26_wseq_write(cs40l26, reg,
				FIELD_GET(CS40L26_WSEQ_UPPER_MASK, new_mask),
				true, CS40L26_WSEQ_OP_WRITE_H16, &cs40l26->pseq);
		if (error)
			return error;
	}

	if (bit_mask & CS40L26_WSEQ_LOWER_MASK) {
		error = cs40l26_wseq_write(cs40l26, reg,
				FIELD_GET(CS40L26_WSEQ_LOWER_MASK, new_mask),
				true, CS40L26_WSEQ_OP_WRITE_L16, &cs40l26->pseq);
		if (error)
			return error;
	}

	return 0;
}

static int cs40l26_map_gpi_to_haptic(struct cs40l26_private *cs40l26, struct ff_effect *effect,
		struct cs40l26_uploaded_effect *ueffect)
{
	u8 gpio = (effect->trigger.button & CS40L26_BTN_NUM_MASK) >> CS40L26_BTN_NUM_SHIFT;
	bool edge, ev_handler_bank_ram, owt, use_timeout;
	unsigned int fw_rev;
	u32 reg, write_val;
	int error;

	if (cs40l26->gpo_playback_mon) {
		dev_err(cs40l26->dev, "Cannot use GPIO1 when playback status monitoring enabled\n");
		ueffect->mapping = CS40L26_GPIO_MAP_INVALID;
		return -EPERM;
	}

	edge = (effect->trigger.button & CS40L26_BTN_EDGE_MASK) >> CS40L26_BTN_EDGE_SHIFT;

	switch (ueffect->wvfrm_bank) {
	case CS40L26_RAM_BANK_ID:
	case CS40L26_BUZ_BANK_ID:
		owt = false;
		ev_handler_bank_ram = true;
		break;
	case CS40L26_ROM_BANK_ID:
		owt = false;
		ev_handler_bank_ram = false;
		break;
	case CS40L26_OWT_BANK_ID:
		owt = true;
		ev_handler_bank_ram = true;
		break;
	default:
		dev_err(cs40l26->dev, "Effect bank %u not supported\n", ueffect->wvfrm_bank);
		return -EINVAL;
	}

	if (gpio != CS40L26_GPIO1) {
		dev_err(cs40l26->dev, "GPIO%u not supported on 0x%02X\n", gpio, cs40l26->revid);
		return -EINVAL;
	}

	reg = cs40l26->event_map_base + (edge ? 0 : 4);
	write_val = (ueffect->trigger_index & CS40L26_BTN_INDEX_MASK) |
			(ev_handler_bank_ram << CS40L26_BTN_BANK_SHIFT) |
			(owt << CS40L26_BTN_OWT_SHIFT);

	error = regmap_write(cs40l26->regmap, reg, write_val);
	if (error) {
		dev_err(cs40l26->dev, "Failed to update event map\n");
		return error;
	}

	error = cl_dsp_fw_rev_get(cs40l26->dsp, &fw_rev);
	if (error)
		return error;

	use_timeout = (!cs40l26->calib_fw && fw_rev >= CS40L26_FW_GPI_TIMEOUT_MIN_REV) ||
			(cs40l26->calib_fw && fw_rev >= CS40L26_FW_GPI_TIMEOUT_CALIB_MIN_REV);

	if (use_timeout) {
		error = cl_dsp_get_reg(cs40l26->dsp, "TIMEOUT_GPI_MS", CL_DSP_XM_UNPACKED_TYPE,
				CS40L26_VIBEGEN_ALGO_ID, &reg);
		if (error)
			return error;

		error = regmap_write(cs40l26->regmap, reg, effect->replay.length);
		if (error)
			dev_warn(cs40l26->dev, "Failed to set GPI timeout, continuing...\n");
	}

	if (edge)
		ueffect->mapping = CS40L26_GPIO_MAP_A_PRESS;
	else
		ueffect->mapping = CS40L26_GPIO_MAP_A_RELEASE;

	return error;
}

static struct cs40l26_uploaded_effect *cs40l26_uploaded_effect_find(struct cs40l26_private *cs40l26,
		int id)
{
	struct list_head *head = &cs40l26->effect_head;
	int uid = -1;
	struct cs40l26_uploaded_effect *ueffect;

	if (list_empty(head)) {
		dev_dbg(cs40l26->dev, "Effect list is empty\n");
		return ERR_PTR(-ENODATA);
	}

	list_for_each_entry(ueffect, head, list) {
		uid = ueffect->id;
		if (uid == id)
			break;
	}

	if (uid != id) {
		dev_dbg(cs40l26->dev, "No such effect (ID = %d)\n", id);
		return ERR_PTR(-EINVAL);
	}

	return ueffect;
}

static struct cs40l26_buzzgen_config cs40l26_buzzgen_configs[] = {
	{
		.duration_name = "BUZZ_EFFECTS2_BUZZ_DURATION",
		.freq_name = "BUZZ_EFFECTS2_BUZZ_FREQ",
		.level_name = "BUZZ_EFFECTS2_BUZZ_LEVEL",
		.effect_id = -1
	},
	{
		.duration_name = "BUZZ_EFFECTS3_BUZZ_DURATION",
		.freq_name = "BUZZ_EFFECTS3_BUZZ_FREQ",
		.level_name = "BUZZ_EFFECTS3_BUZZ_LEVEL",
		.effect_id = -1
	},
	{
		.duration_name = "BUZZ_EFFECTS4_BUZZ_DURATION",
		.freq_name = "BUZZ_EFFECTS4_BUZZ_FREQ",
		.level_name = "BUZZ_EFFECTS4_BUZZ_LEVEL",
		.effect_id = -1
	},
	{
		.duration_name = "BUZZ_EFFECTS5_BUZZ_DURATION",
		.freq_name = "BUZZ_EFFECTS5_BUZZ_FREQ",
		.level_name = "BUZZ_EFFECTS5_BUZZ_LEVEL",
		.effect_id = -1
	},
	{
		.duration_name = "BUZZ_EFFECTS6_BUZZ_DURATION",
		.freq_name = "BUZZ_EFFECTS6_BUZZ_FREQ",
		.level_name = "BUZZ_EFFECTS6_BUZZ_LEVEL",
		.effect_id = -1
	},
};

static int cs40l26_buzzgen_find_slot(struct cs40l26_private *cs40l26, int id)
{
	int i, slot = -1;

	for (i = CS40L26_BUZZGEN_NUM_CONFIGS - 1; i >= 0; i--) {
		if (cs40l26_buzzgen_configs[i].effect_id == id) {
			slot = i;
			break;
		} else if (cs40l26_buzzgen_configs[i].effect_id == -1) {
			slot = i;
		}
	}

	return slot;
}

static int cs40l26_erase_buzzgen(struct cs40l26_private *cs40l26, int id)
{
	int slot = cs40l26_buzzgen_find_slot(cs40l26, id);

	if (slot == -1) {
		dev_err(cs40l26->dev, "Failed to erase BUZZGEN config for id %d\n", id);
		return -EINVAL;
	}

	cs40l26_buzzgen_configs[slot].effect_id = -1;

	return 0;
}

static bool cs40l26_is_no_wait_ram_index(struct cs40l26_private *cs40l26,
		u32 index)
{
	int i;

	for (i = 0; i < cs40l26->num_no_wait_ram_indices; i++) {
		if (cs40l26->no_wait_ram_indices[i] == index)
			return true;
	}

	return false;
}

static void cs40l26_set_gain_worker(struct work_struct *work)
{
	struct cs40l26_private *cs40l26 = container_of(work, struct cs40l26_private, set_gain_work);
	u32 algo_id, reg;
	int error;
	u16 gain;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return;

	mutex_lock(&cs40l26->lock);

	if (cs40l26->vibe_state == CS40L26_VIBE_STATE_ASP) {
		gain = (cs40l26->asp_scale_pct * cs40l26->gain_pct) / CS40L26_GAIN_FULL_SCALE;
		cs40l26->gain_tmp = cs40l26->gain_pct;
		cs40l26->gain_pct = gain;
		cs40l26->scaling_applied = true;
	} else {
		gain = cs40l26->gain_pct;
	}

	dev_dbg(cs40l26->dev, "%s: gain = %u%%\n", __func__, gain);

	/* Write Q21.2 value to SOURCE_ATTENUATION */
	error = cs40l26_get_ram_ext_algo_id(cs40l26, &algo_id);
	if (error)
		goto err_mutex;

	error = cl_dsp_get_reg(cs40l26->dsp, "SOURCE_ATTENUATION",
			CL_DSP_XM_UNPACKED_TYPE, algo_id, &reg);
	if (error)
		goto err_mutex;

	error = regmap_write(cs40l26->regmap, reg, cs40l26_attn_q21_2_vals[gain]);
	if (error)
		dev_err(cs40l26->dev, "Failed to set attenuation\n");

err_mutex:
	mutex_unlock(&cs40l26->lock);
	cs40l26_pm_exit(cs40l26->dev);
}

static void cs40l26_vibe_start_worker(struct work_struct *work)
{
	struct cs40l26_private *cs40l26 = container_of(work, struct cs40l26_private,
			vibe_start_work);
	struct device *dev = cs40l26->dev;
	struct cs40l26_uploaded_effect *ueffect;
	struct ff_effect *effect;
	u32 algo_id, reg;
	u16 duration;
	bool invert;
	int error;

	dev_dbg(dev, "%s: effect ID = %d, duration = %d\n", __func__, cs40l26->trigger_effect->id, cs40l26->trigger_effect->replay.length);

	error = cs40l26_pm_enter(dev);
	if (error)
		return;

	mutex_lock(&cs40l26->lock);

	effect = cs40l26->trigger_effect;

	ueffect = cs40l26_uploaded_effect_find(cs40l26, effect->id);
	if (IS_ERR_OR_NULL(ueffect)) {
		dev_err(dev, "No such effect to play back\n");
		goto err_mutex;
	}

	duration = effect->replay.length;

	error = cl_dsp_get_reg(cs40l26->dsp, "TIMEOUT_MS",
			CL_DSP_XM_UNPACKED_TYPE, CS40L26_VIBEGEN_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_write(cs40l26->regmap, reg, duration);
	if (error) {
		dev_err(dev, "Failed to set TIMEOUT_MS\n");
		goto err_mutex;
	}

	error = cs40l26_get_ram_ext_algo_id(cs40l26, &algo_id);
	if (error)
		goto err_mutex;

	error = cl_dsp_get_reg(cs40l26->dsp, "SOURCE_INVERT",
			CL_DSP_XM_UNPACKED_TYPE, algo_id, &reg);
	if (error)
		goto err_mutex;

	switch (effect->direction) {
	case 0x0000:
		invert = false;
		break;
	case 0x8000:
		invert = true;
		break;
	default:
		dev_err(dev, "Invalid ff_effect direction: 0x%X\n", effect->direction);
		goto err_mutex;
	}

	error = regmap_write(cs40l26->regmap, reg, invert);
	if (error)
		goto err_mutex;

	switch (effect->u.periodic.waveform) {
	case FF_CUSTOM:
	case FF_SINE:
		error = cs40l26_mailbox_write(cs40l26, ueffect->trigger_index);
		if (error)
			goto err_mutex;

		cs40l26->cur_index = ueffect->trigger_index;
		break;
	default:
		dev_err(dev, "Invalid waveform type: 0x%X\n", effect->u.periodic.waveform);
		goto err_mutex;
	}

	if (!cs40l26->vibe_state_reporting)
		cs40l26_vibe_state_update(cs40l26, CS40L26_VIBE_STATE_EVENT_MBOX_PLAYBACK);

	reinit_completion(&cs40l26->erase_cont);
err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(dev);
	
	dev_dbg(dev, "%s: effect ID = %d done\n", __func__, cs40l26->trigger_effect->id);
}

static void cs40l26_vibe_stop_worker(struct work_struct *work)
{
	struct cs40l26_private *cs40l26 = container_of(work, struct cs40l26_private,
			vibe_stop_work);
	bool skip_delay;
	u32 delay_us;
	int error;

	dev_dbg(cs40l26->dev, "%s:\n", __func__);

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return;

	mutex_lock(&cs40l26->lock);

	delay_us = cs40l26->delay_before_stop_playback_us;
	skip_delay = cs40l26_is_no_wait_ram_index(cs40l26, cs40l26->cur_index);

	if (delay_us && !skip_delay) {
		mutex_unlock(&cs40l26->lock);

		dev_info(cs40l26->dev, "Applying delay\n");

		/* wait for SVC init phase to complete */
		usleep_range(delay_us, delay_us + 100);

		mutex_lock(&cs40l26->lock);
	} else {
		dev_info(cs40l26->dev, "Skipping delay\n");
	}

	if (!skip_delay) {
		error = cs40l26_mailbox_write(cs40l26, CS40L26_STOP_PLAYBACK);
		if (error)
			dev_err(cs40l26->dev, "Failed to stop playback\n");
	} else {
		dev_dbg(cs40l26->dev, "Stop command skipped\n");
	}

	mutex_unlock(&cs40l26->lock);
	cs40l26_pm_exit(cs40l26->dev);

	dev_dbg(cs40l26->dev, "%s: done\n", __func__);
}

static void cs40l26_set_gain(struct input_dev *dev, u16 gain)
{
	struct cs40l26_private *cs40l26 = input_get_drvdata(dev);

	dev_dbg(cs40l26->dev, "%s: gain = %u\n", __func__, gain);
	
	if (gain >= CS40L26_NUM_PCT_MAP_VALUES) {
		dev_err(cs40l26->dev, "Gain value %u %% out of bounds\n", gain);
		return;
	}

	cs40l26->gain_pct = gain;

	queue_work(cs40l26->vibe_workqueue, &cs40l26->set_gain_work);
}

static int cs40l26_playback_effect(struct input_dev *dev,
		int effect_id, int val)
{
	struct cs40l26_private *cs40l26 = input_get_drvdata(dev);
	struct ff_effect *effect;

	dev_dbg(cs40l26->dev, "%s: effect ID = %d, val = %d\n", __func__, effect_id, val);

	effect = &dev->ff->effects[effect_id];
	if (!effect) {
		dev_err(cs40l26->dev, "No such effect to playback\n");
		return -EINVAL;
	}

	cs40l26->trigger_effect = effect;

	if (val > 0)
		queue_work(cs40l26->vibe_workqueue, &cs40l26->vibe_start_work);
	else
		queue_work(cs40l26->vibe_workqueue, &cs40l26->vibe_stop_work);

	return 0;
}

static int cs40l26_num_ram_waves(struct cs40l26_private *cs40l26)
{
	u32 num_of_waves, reg;
	int error;

	if (!cl_dsp_algo_is_present(cs40l26->dsp, CS40L26_VIBEGEN_ALGO_ID))
		return 0;

	error = cl_dsp_get_reg(cs40l26->dsp, "NUM_OF_WAVES", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_VIBEGEN_ALGO_ID, &reg);
	if (error)
		return error;

	error = cs40l26_dsp_read(cs40l26, reg, &num_of_waves);
	if (error)
		return error;

	return (int) num_of_waves;
}

static int cs40l26_num_owt_waves(struct cs40l26_private *cs40l26)
{
	u32 owt_num_of_waves, reg;
	int error;

	if (!cl_dsp_algo_is_present(cs40l26->dsp, CS40L26_VIBEGEN_ALGO_ID))
		return 0;

	error = cl_dsp_get_reg(cs40l26->dsp, "OWT_NUM_OF_WAVES_XM", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_VIBEGEN_ALGO_ID, &reg);
	if (error)
		return error;

	error = cs40l26_dsp_read(cs40l26, reg, &owt_num_of_waves);
	if (error)
		return error;

	return (int) owt_num_of_waves;
}

int cs40l26_num_waves(struct cs40l26_private *cs40l26)
{
	int nowt, nram;

	nram = cs40l26_num_ram_waves(cs40l26);
	if (nram < 0)
		return nram;

	nowt = cs40l26_num_owt_waves(cs40l26);
	if (nowt <  0)
		return nowt;

	return nram + nowt;
}
EXPORT_SYMBOL_GPL(cs40l26_num_waves);

static struct cl_dsp_owt_header *cs40l26_owt_header(struct cs40l26_private *cs40l26, u8 index,
		u16 bank)
{
	if (bank == CS40L26_RAM_BANK_ID && cs40l26->dsp->wt_desc &&
			index < cs40l26->dsp->wt_desc->owt.nwaves)
		return &cs40l26->dsp->wt_desc->owt.waves[index];
	if (bank == CS40L26_ROM_BANK_ID && index < cs40l26->rom_wt.nwaves)
		return &cs40l26->rom_wt.waves[index];

	return ERR_PTR(-EINVAL);
}

static int cs40l26_owt_get_wlength(struct cs40l26_private *cs40l26, u8 index, u32 *wlen_whole,
		u16 bank)
{
	struct device *dev = cs40l26->dev;
	struct cl_dsp_owt_header *entry;
	struct cl_dsp_memchunk ch;

	if (index == 0) {
		*wlen_whole = 0;
		return 0;
	}

	entry = cs40l26_owt_header(cs40l26, index, bank);
	if (IS_ERR(entry))
		return PTR_ERR(entry);

	switch (entry->type) {
	case WT_TYPE_V6_PCM_F0_REDC:
	case WT_TYPE_V6_PCM_F0_REDC_VAR:
	case WT_TYPE_V6_PWLE:
		break;
	default:
		dev_err(dev, "Cannot size waveform type %u\n", entry->type);
		return -EINVAL;
	}

	ch = cl_dsp_memchunk_create(entry->data, sizeof(u32));

	/* First 24 bits of each waveform is the length in samples @ 8 kHz */
	return cl_dsp_memchunk_read(cs40l26->dsp, &ch, 24, wlen_whole);
}

static void cs40l26_owt_set_section_info(struct cs40l26_private *cs40l26,
		struct cl_dsp_memchunk *ch, struct cs40l26_owt_section *sections, u8 nsections)
{
	int i;

	for (i = 0; i < nsections; i++) {
		cl_dsp_memchunk_write(ch, 8, sections[i].amplitude);
		cl_dsp_memchunk_write(ch, 8, sections[i].index);
		cl_dsp_memchunk_write(ch, 8, sections[i].repeat);
		cl_dsp_memchunk_write(ch, 8, sections[i].flags);
		cl_dsp_memchunk_write(ch, 16, sections[i].delay);

		if (sections[i].flags & CS40L26_WT_TYPE10_COMP_DURATION_FLAG) {
			cl_dsp_memchunk_write(ch, 8, 0x00); /* Pad */
			cl_dsp_memchunk_write(ch, 16, sections[i].duration);
		}
	}
}

static int cs40l26_owt_get_section_info(struct cs40l26_private *cs40l26, struct cl_dsp_memchunk *ch,
		struct cs40l26_owt_section *sections, u8 nsections)
{
	int error = 0, i;

	for (i = 0; i < nsections; i++) {
		error = cl_dsp_memchunk_read(cs40l26->dsp, ch, 8, &sections[i].amplitude);
		if (error)
			return error;

		error = cl_dsp_memchunk_read(cs40l26->dsp, ch, 8, &sections[i].index);
		if (error)
			return error;

		error = cl_dsp_memchunk_read(cs40l26->dsp, ch, 8, &sections[i].repeat);
		if (error)
			return error;

		error = cl_dsp_memchunk_read(cs40l26->dsp, ch, 8, &sections[i].flags);
		if (error)
			return error;

		error = cl_dsp_memchunk_read(cs40l26->dsp, ch, 16, &sections[i].delay);
		if (error)
			return error;

		if (sections[i].flags & CS40L26_WT_TYPE10_COMP_DURATION_FLAG) {
			/* Skip padding */
			error = cl_dsp_memchunk_read(cs40l26->dsp, ch, 8, NULL);
			if (error)
				return error;

			error = cl_dsp_memchunk_read(cs40l26->dsp, ch, 16, &sections[i].duration);
			if (error)
				return error;
		}

		if (sections[i].flags & CS40L26_WT_TYPE10_COMP_ROM_FLAG)
			sections[i].wvfrm_bank = CS40L26_ROM_BANK_ID;
		else
			sections[i].wvfrm_bank = CS40L26_RAM_BANK_ID;
	}

	return error;
}

static int cs40l26_owt_calculate_wlength(struct cs40l26_private *cs40l26, u8 nsections,
		u8 global_rep, u8 *data, u32 data_size_bytes, u32 *owt_wlen)
{
	u32 total_len = 0, section_len = 0, loop_len = 0, wlen_whole = 0;
	bool in_loop = false;
	struct cs40l26_owt_section *sections;
	struct cl_dsp_memchunk ch;
	u32 dlen, wlen;
	int error, i;

	if (nsections < 1) {
		dev_err(cs40l26->dev, "Not enough sections for composite\n");
		return -EINVAL;
	}

	sections = kcalloc(nsections, sizeof(struct cs40l26_owt_section), GFP_KERNEL);
	if (!sections)
		return -ENOMEM;

	ch = cl_dsp_memchunk_create((void *) data, data_size_bytes);
	error = cs40l26_owt_get_section_info(cs40l26, &ch, sections, nsections);
	if (error) {
		dev_err(cs40l26->dev, "Failed to get section info\n");
		goto err_free;
	}

	for (i = 0; i < nsections; i++) {
		error = cs40l26_owt_get_wlength(cs40l26, sections[i].index, &wlen_whole,
				sections[i].wvfrm_bank);
		if (error) {
			dev_err(cs40l26->dev, "Failed to get wlength for index %u: %d\n",
					sections[i].index, error);
			goto err_free;
		}

		if (wlen_whole & CS40L26_WT_TYPE10_WAVELEN_INDEF) {
			if (!(sections[i].flags & CS40L26_WT_TYPE10_COMP_DURATION_FLAG)) {
				dev_err(cs40l26->dev, "Indefinite entry needs duration\n");
				error = -EINVAL;
				goto err_free;
			}

			wlen = CS40L26_WT_TYPE10_WAVELEN_MAX;
		} else {
			/* Length is 22 LSBs, filter out flags */
			wlen = wlen_whole & CS40L26_WT_TYPE10_WAVELEN_MAX;
		}

		dlen = 8 * sections[i].delay;

		if (sections[i].flags & CS40L26_WT_TYPE10_COMP_DURATION_FLAG) {
			if (wlen > (2 * sections[i].duration))
				wlen = 2 * sections[i].duration;
		}

		section_len = wlen + dlen;
		loop_len += section_len;

		if (sections[i].repeat == 0xFF) {
			in_loop = true;
		} else if (sections[i].repeat) {
			total_len += (loop_len * (sections[i].repeat + 1));

			in_loop = false;
			loop_len = 0;
		} else if (!in_loop) {
			total_len += section_len;
			loop_len = 0;
		}
	}

	*owt_wlen = (total_len * (global_rep + 1)) | CS40L26_WT_TYPE10_WAVELEN_CALCULATED;

err_free:
	kfree(sections);

	return error;
}

static int cs40l26_owt_upload(struct cs40l26_private *cs40l26, u8 *data, u32 data_size_bytes)
{
	struct device *dev = cs40l26->dev;
	struct cl_dsp *dsp = cs40l26->dsp;
	unsigned int write_reg, reg, wt_offset, wt_size_words, wt_base;
	int error;

	error = cs40l26_pm_enter(dev);
	if (error)
		return error;

	error = cl_dsp_get_reg(dsp, "OWT_NEXT_XM", CL_DSP_XM_UNPACKED_TYPE, CS40L26_VIBEGEN_ALGO_ID,
			&reg);
	if (error)
		goto err_pm;

	error = regmap_read(cs40l26->regmap, reg, &wt_offset);
	if (error) {
		dev_err(dev, "Failed to get wavetable offset\n");
		goto err_pm;
	}

	error = cl_dsp_get_reg(dsp, "OWT_SIZE_XM", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_VIBEGEN_ALGO_ID, &reg);
	if (error)
		goto err_pm;

	error = regmap_read(cs40l26->regmap, reg, &wt_size_words);
	if (error) {
		dev_err(dev, "Failed to get available WT size\n");
		goto err_pm;
	}

	if ((wt_size_words * CL_DSP_BYTES_PER_WORD) < data_size_bytes) {
		dev_err(dev, "No space for OWT waveform\n");
		error = -ENOSPC;
		goto err_pm;
	}

	error = cl_dsp_get_reg(dsp, CS40L26_WT_NAME_XM, CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_VIBEGEN_ALGO_ID, &wt_base);
	if (error)
		goto err_pm;

	write_reg = wt_base + (wt_offset * 4);

	error = cl_dsp_raw_write(cs40l26->dsp, write_reg, data, data_size_bytes, CL_DSP_MAX_WLEN);
	if (error) {
		dev_err(dev, "Failed to sync OWT\n");
		goto err_pm;
	}

	error = cs40l26_mailbox_write(cs40l26, CS40L26_DSP_MBOX_CMD_OWT_PUSH);
	if (error)
		goto err_pm;

	dev_dbg(dev, "Successfully wrote waveform (%u bytes) to 0x%08X\n", data_size_bytes,
			write_reg);

err_pm:
	cs40l26_pm_exit(dev);

	return error;
}

static u8 *cs40l26_ncw_refactor_data(struct cs40l26_private *cs40l26, u8 amp, u8 nsections,
		void *in_data, u32 data_bytes, u16 bank)
{
	struct cs40l26_owt_section *sections;
	struct cl_dsp_memchunk in_ch, out_ch;
	u16 amp_product;
	u8 *out_data;
	int i, error;

	if (nsections <= 0) {
		dev_err(cs40l26->dev, "Too few sections for NCW\n");
		return ERR_PTR(-EINVAL);
	}

	sections = kcalloc(nsections, sizeof(struct cs40l26_owt_section), GFP_KERNEL);
	if (!sections)
		return ERR_PTR(-ENOMEM);

	in_ch = cl_dsp_memchunk_create(in_data, data_bytes);

	error = cs40l26_owt_get_section_info(cs40l26, &in_ch, sections, nsections);
	if (error) {
		dev_err(cs40l26->dev, "Failed to get section info\n");
		goto sections_free;
	}

	for (i = 0; i < nsections; i++) {
		if (sections[i].index != 0) {
			amp_product = sections[i].amplitude * amp;
			sections[i].amplitude = (u8) DIV_ROUND_UP(amp_product, 100);
		}
		if (bank == CS40L26_ROM_BANK_ID)
			sections[i].flags |= CS40L26_WT_TYPE10_COMP_ROM_FLAG;
	}

	out_data = kzalloc(data_bytes, GFP_KERNEL);
	if (!out_data) {
		error = -ENOMEM;
		goto sections_free;
	}

	out_ch = cl_dsp_memchunk_create((void *) out_data, data_bytes);
	cs40l26_owt_set_section_info(cs40l26, &out_ch, sections, nsections);

sections_free:
	kfree(sections);

	return error ? ERR_PTR(error) : out_data;
}

static int cs40l26_owt_comp_data_size(struct cs40l26_private *cs40l26,
		u8 nsections, struct cs40l26_owt_section *sections)
{
	int i, size = 0;
	struct cl_dsp_owt_header *header;

	for (i = 0; i < nsections; i++) {
		if (sections[i].index == 0) {
			size += CS40L26_WT_TYPE10_SECTION_BYTES_MIN;
			continue;
		}

		header = cs40l26_owt_header(cs40l26, sections[i].index, sections[i].wvfrm_bank);
		if (IS_ERR(header))
			return PTR_ERR(header);

		if (header->type == WT_TYPE_V6_COMPOSITE) {
			size += (header->size - 2) * 4;

			if (section_complete(&sections[i]))
				size += CS40L26_WT_TYPE10_SECTION_BYTES_MIN;
		} else {
			size += sections[i].duration ?
					CS40L26_WT_TYPE10_SECTION_BYTES_MAX :
					CS40L26_WT_TYPE10_SECTION_BYTES_MIN;
		}
	}

	return size;
}

static int cs40l26_composite_upload(struct cs40l26_private *cs40l26, s16 *in_data,
		u32 in_data_nibbles)
{
	int pos_byte = 0, in_pos_nib = 2, in_data_bytes = 2 * in_data_nibbles;
	u8 nsections, global_rep, out_nsections = 0;
	int out_data_bytes = 0, data_bytes = 0;
	struct device *dev = cs40l26->dev;
	u8 ncw_nsections, ncw_global_rep, *data, *ncw_data, *out_data;
	u8 delay_section_data[CS40L26_WT_TYPE10_SECTION_BYTES_MIN];
	struct cs40l26_owt_section *sections;
	struct cl_dsp_memchunk ch, out_ch;
	struct cl_dsp_owt_header *header;
	u16 section_size_bytes;
	u32 ncw_bytes, wlen;
	int i, error;

	ch = cl_dsp_memchunk_create((void *) in_data, in_data_bytes);
	/* Skip padding */
	error = cl_dsp_memchunk_read(cs40l26->dsp, &ch, 8, NULL);
	if (error)
		return error;

	error = cl_dsp_memchunk_read(cs40l26->dsp, &ch, 8, &nsections);
	if (error)
		return error;

	error = cl_dsp_memchunk_read(cs40l26->dsp, &ch, 8, &global_rep);
	if (error)
		return error;

	sections = kcalloc(nsections, sizeof(struct cs40l26_owt_section),
			GFP_KERNEL);
	if (!sections)
		return -ENOMEM;

	error = cs40l26_owt_get_section_info(cs40l26, &ch, sections, nsections);
	if (error) {
		dev_err(cs40l26->dev, "Failed to get section info\n");
		goto sections_err_free;
	}

	data_bytes = cs40l26_owt_comp_data_size(cs40l26, nsections, sections);
	if (data_bytes <= 0) {
		dev_err(dev, "Failed to get OWT Composite Data Size\n");
		error = data_bytes;
		goto sections_err_free;
	}

	data = kcalloc(data_bytes, sizeof(u8), GFP_KERNEL);
	if (!data) {
		error = -ENOMEM;
		goto sections_err_free;
	}

	cl_dsp_memchunk_flush(&ch);
	memset(&delay_section_data, 0, CS40L26_WT_TYPE10_SECTION_BYTES_MIN);

	for (i = 0; i < nsections; i++) {
		section_size_bytes = sections[i].duration ?
				CS40L26_WT_TYPE10_SECTION_BYTES_MAX :
				CS40L26_WT_TYPE10_SECTION_BYTES_MIN;

		if (sections[i].index == 0) {
			memcpy(data + pos_byte, in_data + in_pos_nib, section_size_bytes);
			pos_byte += section_size_bytes;
			in_pos_nib += section_size_bytes / 2;
			out_nsections++;
			continue;
		}

		if (sections[i].repeat != 0) {
			dev_err(dev, "Inner repeats not allowed for NCWs\n");
			error = -EPERM;
			goto data_err_free;
		}

		header = cs40l26_owt_header(cs40l26, sections[i].index, sections[i].wvfrm_bank);
		if (IS_ERR(header)) {
			error = PTR_ERR(header);
			goto data_err_free;
		}

		if (header->type == WT_TYPE_V6_COMPOSITE) {
			ch = cl_dsp_memchunk_create(header->data, 8);
			/* Skip Wlength */
			error = cl_dsp_memchunk_read(cs40l26->dsp, &ch, 24, NULL);
			if (error)
				goto data_err_free;

			/* Skip Padding */
			error = cl_dsp_memchunk_read(cs40l26->dsp, &ch, 8, NULL);
			if (error)
				goto data_err_free;

			error = cl_dsp_memchunk_read(cs40l26->dsp, &ch, 8, &ncw_nsections);
			if (error)
				goto data_err_free;

			error = cl_dsp_memchunk_read(cs40l26->dsp, &ch, 8, &ncw_global_rep);
			if (error)
				goto data_err_free;

			if (ncw_global_rep != 0) {
				dev_err(dev,
					"No NCW support for outer repeat\n");
				error = -EPERM;
				goto data_err_free;
			}

			cl_dsp_memchunk_flush(&ch);

			ncw_bytes = (header->size - 2) * 4;
			ncw_data = cs40l26_ncw_refactor_data(cs40l26, sections[i].amplitude,
					ncw_nsections, header->data + 8,
					ncw_bytes, sections[i].wvfrm_bank);
			if (IS_ERR(ncw_data)) {
				error = PTR_ERR(ncw_data);
				goto data_err_free;
			}

			memcpy(data + pos_byte, ncw_data, ncw_bytes);
			pos_byte += ncw_bytes;
			out_nsections += ncw_nsections;
			kfree(ncw_data);

			if (section_complete(&sections[i])) {
				ch = cl_dsp_memchunk_create((void *) delay_section_data,
						CS40L26_WT_TYPE10_SECTION_BYTES_MIN);

				cl_dsp_memchunk_write(&ch, 24, 0x000000);
				cl_dsp_memchunk_write(&ch, 8, 0x00);
				cl_dsp_memchunk_write(&ch, 16, sections[i].delay);

				memcpy(data + pos_byte, delay_section_data,
						CS40L26_WT_TYPE10_SECTION_BYTES_MIN);

				cl_dsp_memchunk_flush(&ch);

				pos_byte += CS40L26_WT_TYPE10_SECTION_BYTES_MIN;
				out_nsections++;
			}
		} else {
			memcpy(data + pos_byte, in_data + in_pos_nib, section_size_bytes);
			pos_byte += section_size_bytes;
			out_nsections++;
		}
		in_pos_nib += section_size_bytes / 2;
	}

	out_data_bytes = data_bytes + CS40L26_WT_HEADER_COMP_SIZE;
	out_data = kcalloc(out_data_bytes, sizeof(u8), GFP_KERNEL);
	if (!out_data) {
		dev_err(dev, "Failed to allocate space for composite\n");
		error = -ENOMEM;
		goto data_err_free;
	}

	out_ch = cl_dsp_memchunk_create((void *) out_data, out_data_bytes);
	cl_dsp_memchunk_write(&out_ch, 16, CS40L26_WT_HEADER_DEFAULT_FLAGS);
	cl_dsp_memchunk_write(&out_ch, 8, WT_TYPE_V6_COMPOSITE);
	cl_dsp_memchunk_write(&out_ch, 24, CS40L26_WT_HEADER_OFFSET);
	cl_dsp_memchunk_write(&out_ch, 24, data_bytes / CL_DSP_BYTES_PER_WORD);

	error = cs40l26_owt_calculate_wlength(cs40l26, out_nsections, global_rep, data, data_bytes,
			&wlen);
	if (error)
		goto out_data_err_free;

	cl_dsp_memchunk_write(&out_ch, 24, wlen);
	cl_dsp_memchunk_write(&out_ch, 8, 0x00); /* Pad */
	cl_dsp_memchunk_write(&out_ch, 8, out_nsections);
	cl_dsp_memchunk_write(&out_ch, 8, global_rep);

	memcpy(out_data + out_ch.bytes, data, data_bytes);

	error = cs40l26_owt_upload(cs40l26, out_data, out_data_bytes);

out_data_err_free:
	kfree(out_data);
data_err_free:
	kfree(data);
sections_err_free:
	kfree(sections);

	return error;
}

int cs40l26_rom_wt_init(struct cs40l26_private *cs40l26)
{
	u32 *wt_be, reg, rom_wt_size_bytes;
	int error, i;

	rom_wt_size_bytes = cs40l26->rom_data->rom_wt_size_words * CL_DSP_BYTES_PER_WORD;

	cs40l26->rom_wt.nwaves = cs40l26->rom_data->rom_num_waves;
	cs40l26->rom_wt.raw_data = devm_kzalloc(cs40l26->dev, rom_wt_size_bytes, GFP_KERNEL);
	if (!cs40l26->rom_wt.raw_data)
		return -ENOMEM;

	error = regmap_read(cs40l26->regmap, cs40l26->rom_regs->p_vibegen_rom, &reg);
	if (error)
		goto data_free;

	wt_be = kcalloc(cs40l26->rom_data->rom_wt_size_words, sizeof(u32), GFP_KERNEL);
	if (!wt_be) {
		error = -ENOMEM;
		goto data_free;
	}

	error = regmap_bulk_read(cs40l26->regmap, (reg * CL_DSP_BYTES_PER_WORD) +
			CS40L26_DSP1_XMEM_UNPACKED24_0, wt_be,
			cs40l26->rom_data->rom_wt_size_words);
	if (error)
		goto wt_free;

	for (i = 0; i < cs40l26->rom_wt.nwaves; i++) {
		cs40l26->rom_wt.waves[i].type = *(wt_be + (i * CS40L26_WT_HEADER_OFFSET)) & 0xFF;
		cs40l26->rom_wt.waves[i].offset = *(wt_be + (i * CS40L26_WT_HEADER_OFFSET + 1));
		cs40l26->rom_wt.waves[i].size = *(wt_be + (i * CS40L26_WT_HEADER_OFFSET + 2));
		cs40l26->rom_wt.waves[i].data = (u32 *)cs40l26->rom_wt.raw_data +
				cs40l26->rom_wt.waves[i].offset;
	}

	for (i = 0; i < cs40l26->rom_data->rom_wt_size_words; i++)
		wt_be[i] = be32_to_cpu(wt_be[i]);

	memcpy(cs40l26->rom_wt.raw_data, wt_be, rom_wt_size_bytes);
	kfree(wt_be);

	return 0;
wt_free:
	kfree(wt_be);
data_free:
	devm_kfree(cs40l26->dev, cs40l26->rom_wt.raw_data);
	return error;
}
EXPORT_SYMBOL_GPL(cs40l26_rom_wt_init);

static int cs40l26_sine_upload(struct cs40l26_private *cs40l26, struct ff_effect *effect,
		struct cs40l26_uploaded_effect *ueffect)
{
	unsigned int duration, freq, level;
	int error, slot;
	u32 reg;

	dev_dbg(cs40l26->dev, "%s: effect->id = %d\n", __func__, effect->id);
	
	slot = cs40l26_buzzgen_find_slot(cs40l26, effect->id);
	if (slot == -1) {
		dev_err(cs40l26->dev, "No free BUZZGEN slot available\n");
		return -ENOSPC;
	}

	cs40l26_buzzgen_configs[slot].effect_id = effect->id;

	/*
	 * Divide duration by 4 to match firmware's expectation.
	 * Round up to avoid inadvertently setting a duration of 0.
	 */
	duration = (unsigned int) DIV_ROUND_UP(effect->replay.length, 4);

	if (effect->u.periodic.period < CS40L26_BUZZGEN_PER_MIN)
		freq = 1000 / CS40L26_BUZZGEN_PER_MIN;
	else if (effect->u.periodic.period > CS40L26_BUZZGEN_PER_MAX)
		freq = 1000 / CS40L26_BUZZGEN_PER_MAX;
	else
		freq = 1000 / effect->u.periodic.period;

	if (effect->u.periodic.magnitude < CS40L26_BUZZGEN_LEVEL_MIN)
		level = CS40L26_BUZZGEN_LEVEL_MIN;
	else if (effect->u.periodic.magnitude > CS40L26_BUZZGEN_LEVEL_MAX)
		level = CS40L26_BUZZGEN_LEVEL_MAX;
	else
		level = effect->u.periodic.magnitude;

	error = cl_dsp_get_reg(cs40l26->dsp, cs40l26_buzzgen_configs[slot].duration_name,
			CL_DSP_XM_UNPACKED_TYPE, CS40L26_BUZZGEN_ALGO_ID, &reg);
	if (error)
		return error;

	error = regmap_write(cs40l26->regmap, reg, duration);
	if (error)
		return error;

	error = cl_dsp_get_reg(cs40l26->dsp, cs40l26_buzzgen_configs[slot].freq_name,
			CL_DSP_XM_UNPACKED_TYPE, CS40L26_BUZZGEN_ALGO_ID, &reg);
	if (error)
		return error;

	error = regmap_write(cs40l26->regmap, reg, freq);
	if (error)
		return error;

	error = cl_dsp_get_reg(cs40l26->dsp, cs40l26_buzzgen_configs[slot].level_name,
			CL_DSP_XM_UNPACKED_TYPE, CS40L26_BUZZGEN_ALGO_ID, &reg);
	if (error)
		return error;

	error = regmap_write(cs40l26->regmap, reg, level);
	if (error)
		return error;

	ueffect->id = effect->id;
	ueffect->wvfrm_bank = CS40L26_BUZ_BANK_ID;

	/*
	 * BUZZGEN 1 is reserved for OTP buzz; BUZZGEN 2 - BUZZGEN 6 are valid.
	 * Add an offset of 1 for this reason.
	 */
	ueffect->trigger_index = CS40L26_BUZZGEN_INDEX_START + slot + 1;

	return 0;
}

static int cs40l26_custom_rom(struct cs40l26_private *cs40l26, u32 *trigger_index)
{
	u16 index;

	index = (u16) (cs40l26->raw_custom_data[1] & CS40L26_MAX_INDEX_MASK);

	*trigger_index = index + CS40L26_ROM_INDEX_START;
	if (*trigger_index > CS40L26_ROM_INDEX_END) {
		dev_err(cs40l26->dev, "Index 0x%X out of bounds\n", *trigger_index);
		return -EINVAL;
	}

	return 0;
}

static int cs40l26_custom_ram(struct cs40l26_private *cs40l26, u32 *trigger_index)
{
	int max_index_tmp, nram;
	u32 max_index;
	u16 index;

	index = (u16) (cs40l26->raw_custom_data[1] & CS40L26_MAX_INDEX_MASK);

	nram = cs40l26_num_ram_waves(cs40l26);
	if (nram < 0) {
		return nram;
	} else if (nram == 0) {
		dev_err(cs40l26->dev, "No waveforms in RAM bank\n");
		return -ENODATA;
	}

	max_index_tmp = CS40L26_RAM_INDEX_START + nram - 1;
	max_index = (u32) max_index_tmp;

	*trigger_index = index + CS40L26_RAM_INDEX_START;

	if (*trigger_index > max_index) {
		dev_err(cs40l26->dev, "RAM Index 0x%X out of bounds\n", *trigger_index);
		return -EINVAL;
	}

	return 0;
}

static int cs40l26_custom_owt(struct cs40l26_private *cs40l26, size_t data_len, u32 *trigger_index)
{
	bool pwle = (cs40l26->raw_custom_data[1] == CS40L26_WT_TYPE12_IDENTIFIER);
	u8 *pwle_data = NULL;
	int error, index_tmp, nowt;
	size_t pwle_data_len;
	u16 index;

	if (pwle) {
		pwle_data_len = cs40l26->raw_custom_data_len * 2;

		pwle_data = kcalloc(pwle_data_len, sizeof(u8), GFP_KERNEL);
		if (IS_ERR_OR_NULL(pwle_data)) {
			error = pwle_data ? PTR_ERR(pwle_data) : -ENOMEM;
			return error;
		}

		memcpy(pwle_data, cs40l26->raw_custom_data, pwle_data_len);

		error = cs40l26_owt_upload(cs40l26, pwle_data, pwle_data_len);
		if (error)
			goto err_free;
	} else {
		error = cs40l26_composite_upload(cs40l26, cs40l26->raw_custom_data, data_len);
		if (error)
			goto err_free;
	}

	nowt = cs40l26_num_owt_waves(cs40l26);
	if (nowt < 0) {
		error = nowt;
		goto err_free;
	}

	index_tmp = nowt - 1;
	if (index_tmp < 0) {
		dev_err(cs40l26->dev, "Invalid OWT index: %d\n", index_tmp);
		error = -EINVAL;
		goto err_free;
	}
	index = (u16) (index_tmp & CS40L26_MAX_INDEX_MASK);
	*trigger_index = index + CS40L26_OWT_INDEX_START;

	if (*trigger_index > CS40L26_OWT_INDEX_END) {
		dev_err(cs40l26->dev, "Index 0x%X out of bounds\n", *trigger_index);
		error = -EINVAL;
	}

err_free:
	kfree(pwle_data);

	return error;
}

static int cs40l26_custom_upload(struct cs40l26_private *cs40l26, struct ff_effect *effect,
		struct cs40l26_uploaded_effect *ueffect)
{
	size_t data_len = effect->u.periodic.custom_len;
	u32 trigger_index = 0;
	int error;
	u16 bank;

	if (data_len > CS40L26_CUSTOM_DATA_SIZE) {
		bank = (u16) CS40L26_OWT_BANK_ID;

		error = cs40l26_custom_owt(cs40l26, data_len, &trigger_index);
		if (error)
			return error;
	} else {
		bank = (u16) cs40l26->raw_custom_data[0];

		if (bank == CS40L26_RAM_BANK_ID) {
			error = cs40l26_custom_ram(cs40l26, &trigger_index);
			if (error)
				return error;
		} else if (bank == CS40L26_ROM_BANK_ID) {
			error = cs40l26_custom_rom(cs40l26, &trigger_index);
			if (error)
				return error;
		} else {
			dev_err(cs40l26->dev, "Invalid custom waveform bank: %u\n", bank);
			return -EINVAL;
		}
	}

	ueffect->id = effect->id;
	ueffect->wvfrm_bank = bank;
	ueffect->trigger_index = trigger_index;

	dev_dbg(cs40l26->dev, "ID = %d, trigger index = 0x%08X\n", effect->id, trigger_index);

	return 0;
}

static int cs40l26_uploaded_effect_add(struct cs40l26_private *cs40l26, struct ff_effect *effect)
{
	struct device *dev = cs40l26->dev;
	bool is_new = false;
	struct cs40l26_uploaded_effect *ueffect;
	int error;

	ueffect = cs40l26_uploaded_effect_find(cs40l26, effect->id);
	if (IS_ERR_OR_NULL(ueffect)) {
		is_new = true;
		ueffect = devm_kzalloc(dev, sizeof(*ueffect), GFP_KERNEL);
		if (!ueffect)
			return -ENOMEM;
	}

	if (effect->u.periodic.waveform == FF_CUSTOM) {
		error = cs40l26_custom_upload(cs40l26, effect, ueffect);
	} else if (effect->u.periodic.waveform == FF_SINE) {
		error = cs40l26_sine_upload(cs40l26, effect, ueffect);
	} else {
		dev_err(dev, "Periodic waveform type 0x%X not supported\n",
				effect->u.periodic.waveform);
		error = -EINVAL;
	}

	if (error)
		goto err_free;

	if (effect->trigger.button) {
		error = cs40l26_map_gpi_to_haptic(cs40l26, effect, ueffect);
		if (error && error != -EPERM)
			goto err_free;
	} else {
		ueffect->mapping = CS40L26_GPIO_MAP_INVALID;
	}

	if (is_new)
		list_add(&ueffect->list, &cs40l26->effect_head);

	return 0;
err_free:
	if (is_new)
		devm_kfree(dev, ueffect);

	return error;
}

static void cs40l26_upload_worker(struct work_struct *work)
{
	struct cs40l26_private *cs40l26 = container_of(work,
			struct cs40l26_private, upload_work);
	struct device *cdev = cs40l26->dev;
	struct ff_effect *effect;
	int error, nwaves;

	error = cs40l26_pm_enter(cdev);
	if (error)
		return;

	mutex_lock(&cs40l26->lock);

	effect = &cs40l26->upload_effect;

	if (effect->type != FF_PERIODIC) {
		dev_err(cdev, "Effect type 0x%X not supported\n", effect->type);
		error = -EINVAL;
		goto out_mutex;
	}

	error = cs40l26_uploaded_effect_add(cs40l26, effect);
	if (error)
		goto out_mutex;

	nwaves = cs40l26_num_waves(cs40l26);
	if (nwaves < 0) {
		error = nwaves;
		goto out_mutex;
	}

	dev_dbg(cdev, "Total number of waveforms = %d\n", nwaves);

out_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cdev);

	cs40l26->upload_ret = error;
	
	dev_dbg(cs40l26->dev, "%s: effect ID = %d done, cs40l26->upload_ret = %d\n", __func__, effect->id, cs40l26->upload_ret);
}

static int cs40l26_upload_effect(struct input_dev *dev,
		struct ff_effect *effect, struct ff_effect *old)
{
	struct cs40l26_private *cs40l26 = input_get_drvdata(dev);
	int len = effect->u.periodic.custom_len;
	int error;

	dev_dbg(cs40l26->dev, "%s: effect ID = %d\n", __func__, effect->id);

	memcpy(&cs40l26->upload_effect, effect, sizeof(struct ff_effect));

	if (effect->u.periodic.waveform == FF_CUSTOM) {
		cs40l26->raw_custom_data_len = len;

		cs40l26->raw_custom_data = kcalloc(len, sizeof(s16),
				GFP_KERNEL);
		if (!cs40l26->raw_custom_data) {
			error = -ENOMEM;
			goto out_free;
		}

		if (copy_from_user(cs40l26->raw_custom_data, effect->u.periodic.custom_data,
				sizeof(s16) * len)) {
			dev_err(cs40l26->dev, "Failed to get user data\n");
			error = -EFAULT;
			goto out_free;
		}
	}

	queue_work(cs40l26->vibe_workqueue, &cs40l26->upload_work);

	/* Wait for upload to finish */
	flush_work(&cs40l26->upload_work);

	error = cs40l26->upload_ret;

out_free:
	memset(&cs40l26->upload_effect, 0, sizeof(struct ff_effect));
	kfree(cs40l26->raw_custom_data);
	cs40l26->raw_custom_data = NULL;

	return error;
}

static int cs40l26_erase_gpi_mapping(struct cs40l26_private *cs40l26, enum cs40l26_gpio_map mapping)
{
	u32 reg, base, offset;
	int error;

	if (mapping != CS40L26_GPIO_MAP_A_PRESS && mapping != CS40L26_GPIO_MAP_A_RELEASE) {
		dev_err(cs40l26->dev, "Invalid GPI mapping %u\n", mapping);
		return -EINVAL;
	}

	if (cs40l26->fw_loaded)
		base = cs40l26->event_map_base;
	else
		base = cs40l26->rom_regs->event_map_table_event_data_packed;

	offset = mapping * CL_DSP_BYTES_PER_WORD;
	reg = base + offset;

	error = regmap_write(cs40l26->regmap, reg, CS40L26_EVENT_MAP_GPI_DISABLE);
	if (error) {
		dev_err(cs40l26->dev, "Failed to clear GPI mapping %u\n",
				mapping);
		return error;
	}

	return 0;
}

static int cs40l26_erase_owt(struct cs40l26_private *cs40l26,
		struct cs40l26_uploaded_effect *ueffect)
{
	u32 cmd = CS40L26_DSP_MBOX_CMD_OWT_DELETE_BASE;
	u32 index = ueffect->trigger_index;
	struct cs40l26_uploaded_effect *ueffect_tmp;
	int error;

	cmd |= (index & 0xFF);

	error = cs40l26_mailbox_write(cs40l26, cmd);
	if (error)
		return error;

	/* Update indices for OWT waveforms uploaded after erased effect */
	list_for_each_entry(ueffect_tmp, &cs40l26->effect_head, list) {
		if (ueffect_tmp->wvfrm_bank == CS40L26_OWT_BANK_ID &&
				ueffect_tmp->trigger_index > index)
			ueffect_tmp->trigger_index--;
	}

	return 0;
}

static void cs40l26_erase_worker(struct work_struct *work)
{
	struct cs40l26_private *cs40l26 = container_of(work,
			struct cs40l26_private, erase_work);
	struct cs40l26_uploaded_effect *ueffect;
	int effect_id, error;
	u16 duration;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return;

	mutex_lock(&cs40l26->lock);

	effect_id = cs40l26->erase_effect->id;
	ueffect = cs40l26_uploaded_effect_find(cs40l26, effect_id);
	if (IS_ERR_OR_NULL(ueffect)) {
		dev_err(cs40l26->dev, "No such effect to erase (%d)\n",
				effect_id);
		error = ueffect ? PTR_ERR(ueffect) : -EINVAL;
		goto out_mutex;
	}

	duration = (cs40l26->erase_effect->replay.length == 0) ?
		CS40L26_MAX_WAIT_VIBE_COMPLETE_MS :
		cs40l26->erase_effect->replay.length + CS40L26_ERASE_BUFFER_MS;

	/* Check for ongoing effect playback. */
	if (cs40l26->vibe_state == CS40L26_VIBE_STATE_HAPTIC) {
		/* Wait for effect to complete. */
		mutex_unlock(&cs40l26->lock);
		if (!wait_for_completion_timeout(&cs40l26->erase_cont,
				msecs_to_jiffies(duration))) {
			error = -ETIME;
			dev_err(cs40l26->dev, "Failed to erase effect (%d)\n",
					effect_id);
			goto pm_err;
		}
		mutex_lock(&cs40l26->lock);
	}

	dev_dbg(cs40l26->dev, "%s: effect ID = %d\n", __func__, effect_id);

	if (ueffect->wvfrm_bank == CS40L26_BUZ_BANK_ID) {
		error = cs40l26_erase_buzzgen(cs40l26, ueffect->id);
		if (error)
			goto out_mutex;
	}

	if (ueffect->mapping != CS40L26_GPIO_MAP_INVALID) {
		error = cs40l26_erase_gpi_mapping(cs40l26, ueffect->mapping);
		if (error)
			goto out_mutex;
		ueffect->mapping = CS40L26_GPIO_MAP_INVALID;
	}

	if (ueffect->wvfrm_bank == CS40L26_OWT_BANK_ID)
		error = cs40l26_erase_owt(cs40l26, ueffect);

	if (error) {
		dev_err(cs40l26->dev, "Failed to erase effect: %d", error);
		goto out_mutex;
	}

	list_del(&ueffect->list);
	devm_kfree(cs40l26->dev, ueffect);

out_mutex:
	mutex_unlock(&cs40l26->lock);
pm_err:
	cs40l26_pm_exit(cs40l26->dev);

	cs40l26->erase_ret = error;
	
	dev_dbg(cs40l26->dev, "%s: effect ID = %d done, cs40l26->erase_ret = %d\n", __func__, effect_id, cs40l26->erase_ret);
}

static int cs40l26_erase_effect(struct input_dev *dev, int effect_id)
{
	struct cs40l26_private *cs40l26 = input_get_drvdata(dev);
	struct ff_effect *effect;

	dev_dbg(cs40l26->dev, "%s: effect ID = %d\n", __func__, effect_id);

	effect = &dev->ff->effects[effect_id];
	if (!effect) {
		dev_err(cs40l26->dev, "No such effect to erase\n");
		return -EINVAL;
	}

	cs40l26->erase_effect = effect;

	queue_work(cs40l26->vibe_workqueue, &cs40l26->erase_work);

	/* Wait for erase to finish */
	flush_work(&cs40l26->erase_work);

	return cs40l26->erase_ret;
}

static int cs40l26_input_init(struct cs40l26_private *cs40l26)
{
	struct device *dev = cs40l26->dev;
	int error;

	cs40l26->input = devm_input_allocate_device(dev);
	if (!cs40l26->input)
		return -ENOMEM;

	cs40l26->input->name = "cs40l26_input";
	cs40l26->input->id.product = cs40l26->devid;
	cs40l26->input->id.version = cs40l26->revid;

	input_set_drvdata(cs40l26->input, cs40l26);
	input_set_capability(cs40l26->input, EV_FF, FF_PERIODIC);
	input_set_capability(cs40l26->input, EV_FF, FF_CUSTOM);
	input_set_capability(cs40l26->input, EV_FF, FF_SINE);
	input_set_capability(cs40l26->input, EV_FF, FF_GAIN);

	error = input_ff_create(cs40l26->input, FF_MAX_EFFECTS);
	if (error) {
		dev_err(dev, "Failed to create FF device: %d\n", error);
		return error;
	}

	/*
	 * input_ff_create() automatically sets FF_RUMBLE capabilities;
	 * we want to restrtict this to only FF_PERIODIC
	 */
	clear_bit(FF_RUMBLE, cs40l26->input->ffbit);

	cs40l26->input->ff->upload = cs40l26_upload_effect;
	cs40l26->input->ff->playback = cs40l26_playback_effect;
	cs40l26->input->ff->set_gain = cs40l26_set_gain;
	cs40l26->input->ff->erase = cs40l26_erase_effect;

	error = input_register_device(cs40l26->input);
	if (error) {
		dev_err(dev, "Cannot register input device: %d\n", error);
		return error;
	}

	error = sysfs_create_groups(&cs40l26->input->dev.kobj, cs40l26_attr_groups);
	if (error) {
		dev_err(dev, "Failed to create sysfs groups\n");
		return error;
	}

	cs40l26->vibe_init_success = true;

	return 0;
}

static int cs40l26_part_num_resolve(struct cs40l26_private *cs40l26)
{
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	u32 devid, revid, fullid;
	int error;

	error = regmap_read(regmap, CS40L26_DEVID, &devid);
	if (error) {
		dev_err(dev, "Failed to read device ID\n");
		return error;
	}

	error = regmap_read(regmap, CS40L26_REVID, &revid);
	if (error) {
		dev_err(dev, "Failed to read revision ID\n");
		return error;
	}

	devid &= CS40L26_DEVID_MASK;
	revid &= CS40L26_REVID_MASK;
	fullid = (devid << 8) | revid;

	switch (fullid) {
	case CS40L26_ID_L26A_A1:
	case CS40L26_ID_L27A_A1:
	case CS40L26_ID_L26A_B0:
	case CS40L26_ID_L27A_B0:
	case CS40L26_ID_L27A_B1:
		cs40l26->rom_regs = &cs40l26_rom_regs_a1_b0_b1;
		cs40l26->rom_data = &cs40l26_rom_data_a1_b0_b1;
		break;
	case CS40L26_ID_L27A_B2:
		cs40l26->rom_regs = &cs40l26_rom_regs_b2;
		cs40l26->rom_data = &cs40l26_rom_data_b2;
		break;
	default:
		dev_err(dev, "Invalid ID: 0x%06X 0x%02X\n", devid, revid);
		return -EINVAL;
	}

	cs40l26->devid = devid;
	cs40l26->revid = revid;

	dev_info(dev, "Cirrus Logic %s ID: 0x%06X, Revision: 0x%02X\n",
			CS40L26_DEV_NAME, cs40l26->devid, cs40l26->revid);

	return 0;
}

static int cs40l26_wksrc_config(struct cs40l26_private *cs40l26)
{
	u8 mask_wksrc;
	u32 val, mask;

	if (cs40l26->devid == CS40L26_DEVID_A ||
			cs40l26->devid == CS40L26_DEVID_L27_A)
		mask_wksrc = 1;
	else
		mask_wksrc = 0;

	val = CS40L26_WKSRC_STS_SPI_MASK |
			(mask_wksrc ? CS40L26_WKSRC_STS_GPIO2_MASK : 0) |
			(mask_wksrc ? CS40L26_WKSRC_STS_GPIO3_MASK : 0) |
			(mask_wksrc ? CS40L26_WKSRC_STS_GPIO4_MASK : 0);

	mask = CS40L26_WKSRC_STS_ANY_MASK | CS40L26_WKSRC_STS_GPIO1_MASK |
			CS40L26_WKSRC_STS_I2C_MASK | CS40L26_WKSRC_STS_SPI_MASK |
			CS40L26_WKSRC_STS_GPIO2_MASK | CS40L26_WKSRC_STS_GPIO3_MASK |
			CS40L26_WKSRC_STS_GPIO4_MASK;

	return cs40l26_irq_update_mask(cs40l26, CS40L26_IRQ1_MASK_1, val, mask);
}

static int cs40l26_set_gpio_from_dt(struct cs40l26_private *cs40l26)
{
	u32 reg = cs40l26->event_map_base + (CS40L26_GPIO_MAP_A_PRESS * CL_DSP_BYTES_PER_WORD);
	int error;

	error = regmap_write(cs40l26->regmap, reg, cs40l26->press_idx);
	if (error)
		return error;

	reg += CL_DSP_BYTES_PER_WORD;

	return regmap_write(cs40l26->regmap, reg, cs40l26->release_idx);
}

static int cs40l26_gpio_config(struct cs40l26_private *cs40l26)
{
	u32 irq_val, pad_val, reg;
	int error;

	error = cl_dsp_get_reg(cs40l26->dsp, "ENT_MAP_TABLE_EVENT_DATA_PACKED",
			CL_DSP_XM_UNPACKED_TYPE, CS40L26_EVENT_HANDLER_ALGO_ID,
			&cs40l26->event_map_base);
	if (error)
		return error;

	error = cs40l26_set_gpio_from_dt(cs40l26);
	if (error)
		return error;

	if (cs40l26->gpo_playback_mon) {
		error = regmap_read(cs40l26->regmap, CS40L26_GPIO_PAD_CONTROL, &pad_val);
		if (error)
			return error;

		pad_val |= CS40L26_GP1_CTRL_GPIO << CS40L26_GP1_CTRL_SHIFT;

		error = regmap_write(cs40l26->regmap, CS40L26_GPIO_PAD_CONTROL, pad_val);
		if (error)
			return error;

		error = cs40l26_wseq_write(cs40l26, CS40L26_GPIO_PAD_CONTROL, pad_val, true,
				CS40L26_WSEQ_OP_WRITE_FULL, &cs40l26->pseq);
		if (error)
			return error;

		error = cl_dsp_get_reg(cs40l26->dsp, "GPI_ENABLE_BITMASK", CL_DSP_XM_UNPACKED_TYPE,
				CS40L26_GPIO_ALGO_ID, &reg);
		if (error)
			return error;

		error = regmap_write(cs40l26->regmap, reg, 0);
		if (error)
			return error;

		error = cl_dsp_get_reg(cs40l26->dsp, "GPO_ENABLE_BITMASK", CL_DSP_XM_UNPACKED_TYPE,
				CS40L26_GPIO_ALGO_ID, &reg);
		if (error)
			return error;

		error = regmap_write(cs40l26->regmap, reg, 1);
		if (error)
			return error;

		error = cl_dsp_get_reg(cs40l26->dsp, "SUP_GPI_COUNT", CL_DSP_XM_UNPACKED_TYPE,
				CS40L26_GPIO_ALGO_ID, &reg);
		if (error)
			return error;

		error = regmap_write(cs40l26->regmap, reg, 0);
		if (error)
			return error;
	}

	if (cs40l26->devid == CS40L26_DEVID_A || cs40l26->devid == CS40L26_DEVID_L27_A)
		irq_val = GENMASK(CS40L26_GPIO4_FALL_IRQ, CS40L26_GPIO2_RISE_IRQ);
	else
		irq_val = 0;

	return cs40l26_irq_update_mask(cs40l26, CS40L26_IRQ1_MASK_1, irq_val,
			GENMASK(CS40L26_GPIO4_FALL_IRQ, CS40L26_GPIO1_RISE_IRQ));
}

static const struct cs40l26_brwnout_limits cs40l26_brwnout_params[] = {
	{
		.max = CS40L26_VBBR_THLD_UV_MAX,
		.min = CS40L26_VBBR_THLD_UV_MIN,
	},
	{
		.max = CS40L26_VPBR_THLD_UV_MAX,
		.min = CS40L26_VPBR_THLD_UV_MIN,
	},
	{
		.max = CS40L26_VXBR_MAX_ATT_MAX,
		.min = CS40L26_VXBR_MAX_ATT_MIN,
	},
	{
		.max = CS40L26_VXBR_ATK_STEP_MAX,
		.min = CS40L26_VXBR_ATK_STEP_MIN,
	},
	{
		.max = CS40L26_VXBR_ATK_RATE_MAX,
		.min = CS40L26_VXBR_ATK_RATE_MIN,
	},
	{
		.max = CS40L26_VXBR_WAIT_MAX,
		.min = CS40L26_VXBR_WAIT_MIN,
	},
	{
		.max = CS40L26_VXBR_REL_RATE_MAX,
		.min = CS40L26_VXBR_REL_RATE_MIN,
	},
};

static int cs40l26_brwnout_prevention_init(struct cs40l26_private *cs40l26)
{
	u32 enables, pseq_mask = 0, val, vbbr_config, vpbr_config;
	struct device *dev = cs40l26->dev;
	struct regmap *regmap = cs40l26->regmap;
	int error;

	error = regmap_read(regmap, CS40L26_BLOCK_ENABLES2, &enables);
	if (error) {
		dev_err(dev, "Failed to read block enables 2\n");
		return error;
	}

	enables |= ((cs40l26->vbbr.enable << CS40L26_VBBR_EN_SHIFT) |
			(cs40l26->vpbr.enable << CS40L26_VPBR_EN_SHIFT));

	error = regmap_write(regmap, CS40L26_BLOCK_ENABLES2, enables);
	if (error) {
		dev_err(dev, "Failed to enable brownout prevention\n");
		return error;
	}

	error = cs40l26_wseq_write(cs40l26, CS40L26_BLOCK_ENABLES2, enables, true,
			CS40L26_WSEQ_OP_WRITE_FULL, &cs40l26->pseq);
	if (error)
		return error;

	if (cs40l26->vbbr.enable) {
		pseq_mask = CS40L26_VBBR_ATT_CLR_MASK | CS40L26_VBBR_FLAG_MASK;

		vbbr_config = ((cs40l26->vbbr.thld_uv / CS40L26_VBBR_THLD_UV_DIV) + 1) &
								CS40L26_VBBR_THLD_MASK;

		vbbr_config |= ((cs40l26->vbbr.max_att_db << CS40L26_VXBR_MAX_ATT_SHIFT) &
								CS40L26_VXBR_MAX_ATT_MASK);

		vbbr_config |= ((cs40l26->vbbr.atk_step << CS40L26_VXBR_ATK_STEP_SHIFT) &
								CS40L26_VXBR_ATK_STEP_MASK);

		vbbr_config |= ((cs40l26->vbbr.atk_rate << CS40L26_VXBR_ATK_RATE_SHIFT) &
								CS40L26_VXBR_ATK_RATE_MASK);

		vbbr_config |= ((cs40l26->vbbr.wait << CS40L26_VXBR_WAIT_SHIFT) &
								CS40L26_VXBR_WAIT_MASK);

		vbbr_config |= ((cs40l26->vbbr.rel_rate << CS40L26_VXBR_REL_RATE_SHIFT) &
								CS40L26_VXBR_REL_RATE_MASK);

		error = regmap_read(regmap, CS40L26_VBBR_CONFIG, &val);
		if (error) {
			dev_err(dev, "Failed to read VBBR_CONFIG\n");
			return error;
		}

		vbbr_config |= (val & CS40L26_VXBR_DEFAULT_MASK);

		error = regmap_write(regmap, CS40L26_VBBR_CONFIG, vbbr_config);
		if (error) {
			dev_err(dev, "Failed to write VBBR_CONFIG\n");
			return error;
		}

		error = cs40l26_wseq_write(cs40l26, CS40L26_VBBR_CONFIG,
				(vbbr_config & CS40L26_WSEQ_UPPER_MASK) >> 16,
				true, CS40L26_WSEQ_OP_WRITE_H16, &cs40l26->pseq);
		if (error)
			return error;

		error = cs40l26_wseq_write(cs40l26, CS40L26_VBBR_CONFIG,
				(vbbr_config & CS40L26_WSEQ_LOWER_MASK),
				true, CS40L26_WSEQ_OP_WRITE_L16, &cs40l26->pseq);
		if (error)
			return error;
	}

	if (cs40l26->vpbr.enable) {
		pseq_mask |= CS40L26_VPBR_ATT_CLR_MASK | CS40L26_VPBR_FLAG_MASK;

		vpbr_config = ((cs40l26->vpbr.thld_uv / CS40L26_VPBR_THLD_UV_DIV) - 51) &
								CS40L26_VPBR_THLD_MASK;

		vpbr_config |= ((cs40l26->vpbr.max_att_db << CS40L26_VXBR_MAX_ATT_SHIFT) &
								CS40L26_VXBR_MAX_ATT_MASK);

		vpbr_config |= ((cs40l26->vpbr.atk_step << CS40L26_VXBR_ATK_STEP_SHIFT) &
								CS40L26_VXBR_ATK_STEP_MASK);

		vpbr_config |= ((cs40l26->vpbr.atk_rate << CS40L26_VXBR_ATK_RATE_SHIFT) &
								CS40L26_VXBR_ATK_RATE_MASK);

		vpbr_config |= ((cs40l26->vpbr.wait << CS40L26_VXBR_WAIT_SHIFT) &
								CS40L26_VXBR_WAIT_MASK);

		vpbr_config |= ((cs40l26->vpbr.rel_rate << CS40L26_VXBR_REL_RATE_SHIFT) &
								CS40L26_VXBR_REL_RATE_MASK);

		error = regmap_read(regmap, CS40L26_VPBR_CONFIG, &val);
		if (error) {
			dev_err(dev, "Failed to read VPBR_CONFIG\n");
			return error;
		}

		vpbr_config |= (val & CS40L26_VXBR_DEFAULT_MASK);

		error = regmap_write(regmap, CS40L26_VPBR_CONFIG, vpbr_config);
		if (error) {
			dev_err(dev, "Failed to write VPBR_CONFIG\n");
			return error;
		}

		error = cs40l26_wseq_write(cs40l26, CS40L26_VPBR_CONFIG,
				(vpbr_config & CS40L26_WSEQ_UPPER_MASK) >> 16,
				true, CS40L26_WSEQ_OP_WRITE_H16, &cs40l26->pseq);
		if (error)
			return error;

		error = cs40l26_wseq_write(cs40l26, CS40L26_VPBR_CONFIG,
				(vpbr_config & GENMASK(15, 0)),
				true, CS40L26_WSEQ_OP_WRITE_L16, &cs40l26->pseq);
		if (error)
			return error;
	}

	return cs40l26_irq_update_mask(cs40l26, CS40L26_IRQ1_MASK_2, 0, pseq_mask);
}

static int cs40l26_dc_wd_config(struct cs40l26_private *cs40l26)
{
	u32 wd_config;
	int error;

	if (cs40l26->dc_wd_enabled)
		wd_config = FIELD_PREP(CS40L26_DCIN_WD_EN_MASK, CS40L26_DCIN_WD_ENABLE);
	else
		return 0;

	wd_config |= FIELD_PREP(CS40L26_DCIN_WD_THLD_MASK,
		clamp_val(cs40l26->dc_wd_thld, CS40L26_DCIN_WD_THLD_2P5PCT_FS,
		CS40L26_DCIN_WD_THLD_100P0PCT_FS));

	wd_config |= FIELD_PREP(CS40L26_DCIN_WD_DUR_MASK,
		clamp_val(cs40l26->dc_wd_dur, CS40L26_DCIN_WD_DUR_20_MS,
		CS40L26_DCIN_WD_DUR_4883_MS));

	if (cs40l26->dc_wd_mute)
		wd_config |= FIELD_PREP(CS40L26_DCIN_WD_MODE_MASK, CS40L26_DCIN_WD_MODE_MUTE);
	else
		wd_config |= FIELD_PREP(CS40L26_DCIN_WD_MODE_MASK, CS40L26_DCIN_WD_MODE_NORMAL);

	error = regmap_write(cs40l26->regmap, CS40L26_ALIVE_DCIN_WD, wd_config);
	if (error)
		return error;

	error = cs40l26_wseq_write(cs40l26, CS40L26_ALIVE_DCIN_WD, wd_config, true,
			CS40L26_WSEQ_OP_WRITE_L16, &cs40l26->pseq);
	if (error)
		return error;

	return cs40l26_irq_update_mask(cs40l26, CS40L26_IRQ1_MASK_1, 0,
			CS40L26_DC_WD_RISE_MASK | CS40L26_DC_WD_FALL_MASK);
}

static int cs40l26_asp_config(struct cs40l26_private *cs40l26)
{
	struct reg_sequence *dsp1rx_config =
			kcalloc(2, sizeof(struct reg_sequence), GFP_KERNEL);
	int error;

	if (!dsp1rx_config) {
		dev_err(cs40l26->dev, "Failed to allocate reg. sequence\n");
		return -ENOMEM;
	}

	dsp1rx_config[0].reg = CS40L26_DSP1RX1_INPUT;
	dsp1rx_config[0].def = CS40L26_DATA_SRC_ASPRX1;
	dsp1rx_config[1].reg = CS40L26_DSP1RX5_INPUT;
	dsp1rx_config[1].def = CS40L26_DATA_SRC_ASPRX2;

	error = regmap_multi_reg_write(cs40l26->regmap, dsp1rx_config, 2);
	if (error) {
		dev_err(cs40l26->dev, "Failed to configure ASP\n");
		goto err_free;
	}

	error = cs40l26_wseq_multi_write(cs40l26, dsp1rx_config, 2, true,
			CS40L26_WSEQ_OP_WRITE_L16, &cs40l26->pseq);

err_free:
	kfree(dsp1rx_config);

	return error;
}

static int cs40l26_bst_dcm_config(struct cs40l26_private *cs40l26)
{
	int error = 0;
	u32 val;

	if (cs40l26->bst_dcm_en != CS40L26_BST_DCM_EN_DEFAULT) {
		error = regmap_read(cs40l26->regmap, CS40L26_BST_DCM_CTL, &val);
		if (error)
			return error;

		val &= ~CS40L26_BST_DCM_EN_MASK;
		val |= cs40l26->bst_dcm_en << CS40L26_BST_DCM_EN_SHIFT;

		error = regmap_write(cs40l26->regmap, CS40L26_BST_DCM_CTL, val);
		if (error)
			return error;

		error = cs40l26_wseq_write(cs40l26, CS40L26_BST_DCM_CTL,
				val, true, CS40L26_WSEQ_OP_WRITE_FULL, &cs40l26->pseq);
	}

	return error;
}

static int cs40l26_zero_cross_config(struct cs40l26_private *cs40l26)
{
	int error = 0;
	u32 reg;

	if (cs40l26->pwle_zero_cross) {
		error = cl_dsp_get_reg(cs40l26->dsp, "PWLE_EXTEND_ZERO_CROSS",
				CL_DSP_XM_UNPACKED_TYPE, CS40L26_VIBEGEN_ALGO_ID, &reg);
		if (error)
			return error;

		error = regmap_write(cs40l26->regmap, reg, 1);
		if (error)
			dev_err(cs40l26->dev, "Failed to set PWLE_EXTEND_ZERO_CROSS\n");

	}

	return error;
}

static int cs40l26_calib_dt_config(struct cs40l26_private *cs40l26)
{
	int error = 0;
	u32 reg;

	if (cs40l26->f0_default <= CS40L26_F0_EST_MAX &&
			cs40l26->f0_default >= CS40L26_F0_EST_MIN) {
		error = cl_dsp_get_reg(cs40l26->dsp, "F0_OTP_STORED",
				CL_DSP_XM_UNPACKED_TYPE,
				CS40L26_VIBEGEN_ALGO_ID, &reg);
		if (error)
			return error;

		error = regmap_write(cs40l26->regmap, reg, cs40l26->f0_default);
		if (error) {
			dev_err(cs40l26->dev, "Failed to write default f0\n");
			return error;
		}
	}

	if (cs40l26->redc_default && cs40l26->redc_default <= CS40L26_UINT_24_BITS_MAX) {
		error = cl_dsp_get_reg(cs40l26->dsp, "REDC_OTP_STORED", CL_DSP_XM_UNPACKED_TYPE,
				CS40L26_VIBEGEN_ALGO_ID, &reg);
		if (error)
			return error;

		error = regmap_write(cs40l26->regmap, reg, cs40l26->redc_default);
		if (error) {
			dev_err(cs40l26->dev, "Failed to write default ReDC\n");
			return error;
		}
	}

	if (cs40l26->revid < CS40L26_REVID_B2 &&
			cl_dsp_algo_is_present(cs40l26->dsp, CS40L26_VIBEGEN_ALGO_ID)) {
		if (cs40l26->q_default <= CS40L26_Q_EST_MAX) {
			error = cl_dsp_get_reg(cs40l26->dsp, "Q_STORED", CL_DSP_XM_UNPACKED_TYPE,
					CS40L26_VIBEGEN_ALGO_ID, &reg);
			if (error)
				return error;

			error = regmap_write(cs40l26->regmap, reg, cs40l26->q_default);
			if (error) {
				dev_err(cs40l26->dev, "Failed to write default Q\n");
				return error;
			}
		}
	}

	return error;
}

static int cs40l26_bst_ipk_config(struct cs40l26_private *cs40l26)
{
	u32 bst_ipk;
	int error;

	if (cs40l26->bst_ipk < CS40L26_BST_IPK_UA_MIN || cs40l26->bst_ipk > CS40L26_BST_IPK_UA_MAX)
		bst_ipk = CS40L26_BST_IPK_DEFAULT;
	else
		bst_ipk = (cs40l26->bst_ipk / CS40L26_BST_IPK_UA_STEP) - 16;

	error = regmap_write(cs40l26->regmap, CS40L26_BST_IPK_CTL, bst_ipk);
	if (error) {
		dev_err(cs40l26->dev, "Failed to update BST peak current\n");
		return error;
	}

	error = cs40l26_wseq_write(cs40l26, CS40L26_BST_IPK_CTL, bst_ipk, true,
			CS40L26_WSEQ_OP_WRITE_L16, &cs40l26->pseq);
	if (error)
		return error;

	return cs40l26_irq_update_mask(cs40l26, CS40L26_IRQ1_MASK_1, 0,
			CS40L26_BST_IPK_FLAG_MASK);
}

static int cs40l26_bst_ctl_config(struct cs40l26_private *cs40l26)
{
	u32 bst_ctl, vbst_ctl_2;
	int error;

	if (cs40l26->bst_ctl < CS40L26_BST_UV_MIN || cs40l26->bst_ctl > CS40L26_BST_UV_MAX)
		bst_ctl = CS40L26_BST_CTL_DEFAULT;
	else
		bst_ctl = (cs40l26->bst_ctl - CS40L26_BST_UV_MIN) / CS40L26_BST_UV_STEP;

	error = regmap_write(cs40l26->regmap, CS40L26_VBST_CTL_1, bst_ctl);
	if (error) {
		dev_err(cs40l26->dev, "Failed to write VBST limit\n");
		return error;
	}

	error = cs40l26_wseq_write(cs40l26, CS40L26_VBST_CTL_1, bst_ctl, true,
			CS40L26_WSEQ_OP_WRITE_L16, &cs40l26->pseq);
	if (error)
		return error;

	error = regmap_read(cs40l26->regmap, CS40L26_VBST_CTL_2, &vbst_ctl_2);
	if (error)
		return error;

	vbst_ctl_2 |= FIELD_PREP(CS40L26_BST_CTL_LIM_EN_MASK, 1);

	error = regmap_write(cs40l26->regmap, CS40L26_VBST_CTL_2, vbst_ctl_2);
	if (error)
		return error;

	return cs40l26_wseq_write(cs40l26, CS40L26_VBST_CTL_2, vbst_ctl_2, true,
			CS40L26_WSEQ_OP_WRITE_FULL, &cs40l26->pseq);
}

static int cs40l26_noise_gate_config(struct cs40l26_private *cs40l26)
{
	u32 ng_config;
	u8 enable;
	int error;

	if (cs40l26->ng_thld < CS40L26_NG_THRESHOLD_MIN ||
			cs40l26->ng_thld > CS40L26_NG_THRESHOLD_MAX)
		cs40l26->ng_thld = CS40L26_NG_THRESHOLD_DEFAULT;

	if (cs40l26->ng_delay < CS40L26_NG_DELAY_MIN || cs40l26->ng_delay > CS40L26_NG_DELAY_MAX)
		cs40l26->ng_delay = CS40L26_NG_DELAY_DEFAULT;

	/* Disable noise gate during calibration on 0xB2 */
	if (cs40l26->calib_fw && cs40l26->revid == CS40L26_REVID_B2)
		enable = 0;
	else
		enable = cs40l26->ng_enable;

	ng_config = FIELD_PREP(CS40L26_NG_THRESHOLD_MASK, cs40l26->ng_thld) |
			FIELD_PREP(CS40L26_NG_DELAY_MASK, cs40l26->ng_delay) |
			FIELD_PREP(CS40L26_NG_ENABLE_MASK, enable);

	error = regmap_write(cs40l26->regmap, CS40L26_NG_CONFIG, ng_config);
	if (error)
		return error;

	return cs40l26_wseq_write(cs40l26, CS40L26_NG_CONFIG, ng_config, true,
			CS40L26_WSEQ_OP_WRITE_FULL, &cs40l26->pseq);
}

static int cs40l26_aux_noise_gate_config(struct cs40l26_private *cs40l26)
{
	u32 aux_ng_config;
	int error;

	if (cs40l26->aux_ng_thld > CS40L26_AUX_NG_THLD_MAX)
		cs40l26->aux_ng_thld = CS40L26_AUX_NG_THLD_DEFAULT;

	if (cs40l26->aux_ng_delay > CS40L26_AUX_NG_HOLD_MAX)
		cs40l26->aux_ng_delay = CS40L26_AUX_NG_HOLD_DEFAULT;

	aux_ng_config = FIELD_PREP(CS40L26_AUX_NG_THLD_MASK, cs40l26->aux_ng_thld) |
			FIELD_PREP(CS40L26_AUX_NG_HOLD_MASK, cs40l26->aux_ng_delay) |
			FIELD_PREP(CS40L26_AUX_NG_EN_MASK, cs40l26->aux_ng_enable);

	error = regmap_write(cs40l26->regmap, CS40L26_MIXER_NGATE_CH1_CFG, aux_ng_config);
	if (error)
		return error;

	error = cs40l26_wseq_write(cs40l26, CS40L26_MIXER_NGATE_CH1_CFG, aux_ng_config, true,
			CS40L26_WSEQ_OP_WRITE_FULL, &cs40l26->pseq);
	if (error)
		return error;

	if (cs40l26->revid == CS40L26_REVID_B2)
		return cs40l26_wseq_write(cs40l26, CS40L26_MIXER_NGATE_CH1_CFG,
				CS40L26_AUX_NG_DEFAULT, true, CS40L26_WSEQ_OP_WRITE_FULL,
				&cs40l26->aseq);
	else
		return 0;
}

static int cs40l26_clip_lvl_config(struct cs40l26_private *cs40l26)
{
	u32 clip_lvl, digpwm_config;
	int error;

	error = regmap_write(cs40l26->regmap, CS40L26_TEST_KEY_CTRL, CS40L26_TEST_KEY_UNLOCK_CODE1);
	if (error)
		return error;

	error = cs40l26_wseq_write(cs40l26, CS40L26_TEST_KEY_CTRL, CS40L26_TEST_KEY_UNLOCK_CODE1,
			false, CS40L26_WSEQ_OP_WRITE_L16, &cs40l26->pseq);
	if (error)
		return error;

	error = regmap_write(cs40l26->regmap, CS40L26_TEST_KEY_CTRL, CS40L26_TEST_KEY_UNLOCK_CODE2);
	if (error)
		return error;

	error = cs40l26_wseq_write(cs40l26, CS40L26_TEST_KEY_CTRL, CS40L26_TEST_KEY_UNLOCK_CODE2,
			false, CS40L26_WSEQ_OP_WRITE_ADDR8, &cs40l26->pseq);
	if (error)
		return error;

	if (cs40l26->clip_lvl < CS40L26_CLIP_LVL_UV_MIN ||
			cs40l26->clip_lvl > CS40L26_CLIP_LVL_UV_MAX)
		clip_lvl = CS40L26_CLIP_LVL_DEFAULT;
	else
		clip_lvl = cs40l26->clip_lvl / CS40L26_CLIP_LVL_UV_STEP;

	error = regmap_read(cs40l26->regmap, CS40L26_DIGPWM_CONFIG2, &digpwm_config);
	if (error) {
		dev_err(cs40l26->dev, "Failed to get DIGPWM config\n");
		return error;
	}

	digpwm_config &= ~CS40L26_CLIP_LVL_MASK;
	digpwm_config |= ((clip_lvl << CS40L26_CLIP_LVL_SHIFT) & CS40L26_CLIP_LVL_MASK);

	error = regmap_write(cs40l26->regmap, CS40L26_DIGPWM_CONFIG2, digpwm_config);
	if (error) {
		dev_err(cs40l26->dev, "Failed to set DIGPWM config\n");
		return error;
	}

	error = cs40l26_wseq_write(cs40l26, CS40L26_DIGPWM_CONFIG2, digpwm_config, false,
			CS40L26_WSEQ_OP_WRITE_FULL, &cs40l26->pseq);
	if (error)
		return error;

	error = regmap_write(cs40l26->regmap, CS40L26_TEST_KEY_CTRL, CS40L26_TEST_KEY_LOCK_CODE);
	if (error)
		return error;

	return cs40l26_wseq_write(cs40l26, CS40L26_TEST_KEY_CTRL, CS40L26_TEST_KEY_LOCK_CODE,
			false, CS40L26_WSEQ_OP_WRITE_L16, &cs40l26->pseq);
}

static int cs40l26_lbst_short_test(struct cs40l26_private *cs40l26)
{
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	unsigned int err, vbst_ctl_1, vbst_ctl_2;
	int error;

	error = regmap_read(regmap, CS40L26_VBST_CTL_1, &vbst_ctl_1);
	if (error) {
		dev_err(dev, "Failed to read VBST_CTL_1\n");
		return error;
	}

	error = regmap_read(regmap, CS40L26_VBST_CTL_2, &vbst_ctl_2);
	if (error) {
		dev_err(dev, "Failed to read VBST_CTL_2\n");
		return error;
	}

	error = regmap_update_bits(regmap, CS40L26_VBST_CTL_1,
				 CS40L26_BST_CTL_MASK, CS40L26_BST_CTL_VP);
	if (error) {
		dev_err(dev, "Failed to set VBST_CTL_1\n");
		return error;
	}

	error = regmap_update_bits(regmap, CS40L26_VBST_CTL_2,
			CS40L26_BST_CTL_SEL_MASK, CS40L26_BST_CTL_SEL_FIXED);
	if (error) {
		dev_err(dev, "Failed to set VBST_CTL_2\n");
		return error;
	}

	/* Set GLOBAL_EN; safe because DSP is guaranteed to be off here */
	error = regmap_update_bits(regmap, CS40L26_GLOBAL_ENABLES,
			CS40L26_GLOBAL_EN_MASK, 1);
	if (error) {
		dev_err(dev, "Failed to set GLOBAL_EN\n");
		return error;
	}

	/* Wait until boost converter is guranteed to be powered up */
	usleep_range(CS40L26_BST_TIME_MIN_US, CS40L26_BST_TIME_MAX_US);

	error = regmap_read(regmap, CS40L26_ERROR_RELEASE, &err);
	if (error) {
		dev_err(dev, "Failed to get ERROR_RELEASE contents\n");
		return error;
	}

	if (err & BIT(CS40L26_BST_SHORT_ERR_RLS)) {
		dev_alert(dev, "FATAL: Boost shorted at startup\n");
		return -ENOTRECOVERABLE;
	}

	/* Clear GLOBAL_EN; safe because DSP is guaranteed to be off here */
	error = regmap_update_bits(regmap, CS40L26_GLOBAL_ENABLES,
			CS40L26_GLOBAL_EN_MASK, 0);
	if (error) {
		dev_err(dev, "Failed to clear GLOBAL_EN\n");
		return error;
	}

	error = regmap_write(regmap, CS40L26_VBST_CTL_1, vbst_ctl_1);
	if (error) {
		dev_err(dev, "Failed to set VBST_CTL_1\n");
		return error;
	}

	error = regmap_write(regmap, CS40L26_VBST_CTL_2, vbst_ctl_2);
	if (error)
		dev_err(dev, "Failed to set VBST_CTL_2\n");

	return error;
}

static int cs40l26_handle_a1_errata(struct cs40l26_private *cs40l26)
{
	int error, num_writes;

	if (!cs40l26->expl_mode_enabled) {
		error = cs40l26_lbst_short_test(cs40l26);
		if (error)
			return error;

		num_writes = CS40L26_ERRATA_A1_NUM_WRITES;
	} else {
		num_writes = CS40L26_ERRATA_A1_EXPL_EN_NUM_WRITES;
	}

	return cs40l26_wseq_multi_write(cs40l26, cs40l26_a1_errata, num_writes,
			false, CS40L26_WSEQ_OP_WRITE_FULL, &cs40l26->pseq);
}

#if !defined(FORCE_DISABLE_DBC)
static int cs40l26_dbc_set(struct cs40l26_private *cs40l26, enum cs40l26_dbc_type dbc, u32 val)
{
	u32 algo_id, reg, write_val;
	int error;

	if (val > cs40l26_dbc_params[dbc].max)
		write_val = cs40l26_dbc_params[dbc].max;
	else
		write_val = val;

	error = cs40l26_get_ram_ext_algo_id(cs40l26, &algo_id);
	if (error)
		return error;

	error = cl_dsp_get_reg(cs40l26->dsp, cs40l26_dbc_params[dbc].name, CL_DSP_XM_UNPACKED_TYPE,
			algo_id, &reg);
	if (error)
		return error;

	return regmap_write(cs40l26->regmap, reg, write_val);
}

static int cs40l26_dbc_enable(struct cs40l26_private *cs40l26)
{
	u32 algo_id, reg;
	int error;

	error = cs40l26_get_ram_ext_algo_id(cs40l26, &algo_id);
	if (error)
		return error;

	error = cl_dsp_get_reg(cs40l26->dsp, "FLAGS", CL_DSP_XM_UNPACKED_TYPE,
			algo_id, &reg);
	if (error)
		return error;

	return regmap_update_bits(cs40l26->regmap, reg, CS40L26_DBC_ENABLE_MASK,
			1 << CS40L26_DBC_ENABLE_SHIFT);
}

static int cs40l26_dbc_config(struct cs40l26_private *cs40l26)
{
	int error, i;

	for (i = 0; i < CS40L26_DBC_NUM_CONTROLS; i++) {
		if (cs40l26->dbc_configs[i] == CS40L26_DBC_DEFAULT)
			continue;

		error = cs40l26_dbc_set(cs40l26, (enum cs40l26_dbc_type) i,
				cs40l26->dbc_configs[i]);
		if (error)
			return error;
	}

	return cs40l26->dbc_enable ? cs40l26_dbc_enable(cs40l26) : 0;
}
#endif

static int cs40l26_logger_src_add(struct cs40l26_private *cs40l26,
		enum cs40l26_logger_src_sign sign, enum cs40l26_logger_src_size size,
		enum cs40l26_logger_src_type type, enum cs40l26_logger_src_id id, u32 addr)
{
	u32 offset, reg, src;
	int error;

	src = FIELD_PREP(CS40L26_LOGGER_SRC_ADDR_MASK, addr) |
			FIELD_PREP(CS40L26_LOGGER_SRC_ID_MASK, id) |
			FIELD_PREP(CS40L26_LOGGER_SRC_TYPE_MASK, type) |
			FIELD_PREP(CS40L26_LOGGER_SRC_SIZE_MASK, size) |
			FIELD_PREP(CS40L26_LOGGER_SRC_SIGN_MASK, sign);

	error = cl_dsp_get_reg(cs40l26->dsp, "SOURCE", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_LOGGER_ALGO_ID, &reg);
	if (error)
		return error;

	offset = cs40l26->num_log_srcs * CL_DSP_BYTES_PER_WORD;

	error = regmap_write(cs40l26->regmap, reg + offset, src);
	if (error)
		return error;

	cs40l26->num_log_srcs++;

	error = cl_dsp_get_reg(cs40l26->dsp, "COUNT", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_LOGGER_ALGO_ID, &reg);
	if (error)
		return error;

	return regmap_write(cs40l26->regmap, reg, cs40l26->num_log_srcs);
}

static int cs40l26_logger_setup(struct cs40l26_private *cs40l26)
{
	enum cs40l26_logger_src_type ep_src_type;
	u32 exc_reg, imon_reg, reg, src;
	int error, i;

	if (cs40l26->log_srcs != NULL) {
		cs40l26->num_log_srcs = 0;
		devm_kfree(cs40l26->dev, cs40l26->log_srcs);
	}

	error = cl_dsp_get_reg(cs40l26->dsp, "COUNT", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_LOGGER_ALGO_ID, &reg);
	if (error)
		return error;

	error = regmap_read(cs40l26->regmap, reg, &cs40l26->num_log_srcs);
	if (error)
		return error;

	if (cl_dsp_algo_is_present(cs40l26->dsp, CS40L26_EP_ALGO_ID)) {
		error = cl_dsp_get_reg(cs40l26->dsp, "DBG_SRC_CFG", CL_DSP_XM_UNPACKED_TYPE,
				CS40L26_EP_ALGO_ID, &reg);
		if (error)
			return error;

		error = regmap_write(cs40l26->regmap, reg, CS40L26_LOGGER_SRC_PROTECTION_OUT << 8);
		if (error)
			return error;

		error = cl_dsp_get_reg(cs40l26->dsp, "DBG_ADDR", CL_DSP_XM_UNPACKED_TYPE,
				CS40L26_EP_ALGO_ID, &exc_reg);
		if (error)
			return error;

		exc_reg += CL_DSP_BYTES_PER_WORD;
		exc_reg /= CL_DSP_BYTES_PER_WORD;

		ep_src_type = cs40l26->revid == CS40L26_REVID_B2 ?
				CS40L26_LOGGER_SRC_TYPE_XM_TO_YM : CS40L26_LOGGER_SRC_TYPE_XM_TO_XM;

		error = cs40l26_logger_src_add(cs40l26, CS40L26_LOGGER_SRC_SIGN_SIGNED,
				CS40L26_LOGGER_SRC_SIZE_BLOCK, ep_src_type,
				CS40L26_LOGGER_SRC_ID_EP, exc_reg);
		if (error)
			return error;
	}

	error = cl_dsp_get_reg(cs40l26->dsp, "LOGGER_IMON", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_EXT_ALGO_ID, &imon_reg);
	if (error)
		return error;

	error = cs40l26_logger_src_add(cs40l26, CS40L26_LOGGER_SRC_SIGN_SIGNED,
			CS40L26_LOGGER_SRC_SIZE_BLOCK, CS40L26_LOGGER_SRC_TYPE_XM_TO_XM,
			CS40L26_LOGGER_SRC_ID_IMON, imon_reg / 4);
	if (error)
		return error;

	cs40l26->log_srcs = devm_kcalloc(cs40l26->dev, cs40l26->num_log_srcs,
			sizeof(struct cs40l26_log_src), GFP_KERNEL);
	if (IS_ERR_OR_NULL(cs40l26->log_srcs))
		return cs40l26->log_srcs ? PTR_ERR(cs40l26->log_srcs) : -ENOMEM;

	error = cl_dsp_get_reg(cs40l26->dsp, "SOURCE", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_LOGGER_ALGO_ID, &reg);
	if (error)
		goto err_free;

	for (i = 0; i < cs40l26->num_log_srcs; i++) {
		error = regmap_read(cs40l26->regmap, reg + (i * CL_DSP_BYTES_PER_WORD), &src);
		if (error)
			goto err_free;

		cs40l26->log_srcs[i].sign = FIELD_GET(CS40L26_LOGGER_SRC_SIGN_MASK, src);
		cs40l26->log_srcs[i].size = FIELD_GET(CS40L26_LOGGER_SRC_SIZE_MASK, src);
		cs40l26->log_srcs[i].type = FIELD_GET(CS40L26_LOGGER_SRC_TYPE_MASK, src);
		cs40l26->log_srcs[i].id = FIELD_GET(CS40L26_LOGGER_SRC_ID_MASK, src);
		cs40l26->log_srcs[i].addr = FIELD_GET(CS40L26_LOGGER_SRC_ADDR_MASK, src);
	}

	return 0;

err_free:
	devm_kfree(cs40l26->dev, cs40l26->log_srcs);
	return error;
}

static int cs40l26_dsp_config(struct cs40l26_private *cs40l26)
{
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	unsigned int val;
	u32 reg, value;
	int error;

	if (!cs40l26->fw_rom_only) {
		error = regmap_update_bits(regmap, CS40L26_PWRMGT_CTL,
				CS40L26_MEM_RDY_MASK, 1 << CS40L26_MEM_RDY_SHIFT);
		if (error) {
			dev_err(dev, "Failed to set MEM_RDY to initialize RAM\n");
			return error;
		}

		error = cl_dsp_get_reg(cs40l26->dsp, "CALL_RAM_INIT", CL_DSP_XM_UNPACKED_TYPE,
				cs40l26->fw_id, &reg);
		if (error)
			return error;

		error = cs40l26_dsp_write(cs40l26, reg, 1);
		if (error)
			return error;
	}

	cs40l26->fw_loaded = true;

#ifdef CONFIG_DEBUG_FS
	cs40l26_debugfs_init(cs40l26);
#endif

	error = cs40l26_wseq_init(cs40l26);
	if (error)
		return error;

	if (cs40l26->revid < CS40L26_REVID_B2) {
		error = cs40l26_handle_a1_errata(cs40l26);
		if (error)
			return error;
	}

	if (!cs40l26->fw_rom_only) {
		error = cs40l26_dsp_start(cs40l26);
		if (error)
			return error;
	}

	error = cs40l26_pm_state_transition(cs40l26, CS40L26_PM_STATE_PREVENT_HIBERNATE);
	if (error)
		return error;

	/* ensure firmware running */
	error = cl_dsp_get_reg(cs40l26->dsp, "HALO_STATE", CL_DSP_XM_UNPACKED_TYPE, cs40l26->fw_id,
			&reg);
	if (error)
		return error;

	error = regmap_read(regmap, reg, &val);
	if (error) {
		dev_err(dev, "Failed to read HALO_STATE\n");
		return error;
	}

	if (val != CS40L26_DSP_HALO_STATE_RUN) {
		dev_err(dev, "Firmware in unexpected state: 0x%X\n", val);
		return -EINVAL;
	}

	error = cs40l26_irq_update_mask(cs40l26, CS40L26_IRQ1_MASK_1, 0,
			CS40L26_AMP_ERR_MASK | CS40L26_TEMP_ERR_MASK |
			CS40L26_BST_SHORT_ERR_MASK | CS40L26_BST_DCM_UVP_ERR_MASK |
			CS40L26_BST_OVP_ERR_MASK | CS40L26_VIRTUAL2_MBOX_WR_MASK);
	if (error)
		return error;

	error = cs40l26_wksrc_config(cs40l26);
	if (error)
		return error;

	error = cs40l26_gpio_config(cs40l26);
	if (error)
		return error;

	error = cs40l26_bst_dcm_config(cs40l26);
	if (error)
		return error;

	error = cs40l26_bst_ipk_config(cs40l26);
	if (error)
		return error;

	error = cs40l26_bst_ctl_config(cs40l26);
	if (error)
		return error;

	error = cs40l26_clip_lvl_config(cs40l26);
	if (error)
		return error;

#if !defined(FORCE_DISABLE_DBC)
	if (!cs40l26->dbc_tuning_loaded) {
		error = cs40l26_dbc_config(cs40l26);
		if (error)
			return error;
	} else {
		error = cs40l26_dbc_enable(cs40l26);
		if (error)
			return error;
	}
#endif

	error = cs40l26_zero_cross_config(cs40l26);
	if (error)
		return error;

	error = cs40l26_noise_gate_config(cs40l26);
	if (error)
		return error;

	error = cs40l26_aux_noise_gate_config(cs40l26);
	if (error)
		return error;

	if (!cs40l26->vibe_init_success) {
		error = cs40l26_calib_dt_config(cs40l26);
		if (error)
			return error;
	}

	error = cs40l26_brwnout_prevention_init(cs40l26);
	if (error)
		return error;

	error = cs40l26_dc_wd_config(cs40l26);
	if (error)
		return error;

	cs40l26_pm_runtime_setup(cs40l26);

	error = cs40l26_pm_state_transition(cs40l26, CS40L26_PM_STATE_ALLOW_HIBERNATE);
	if (error)
		return error;

	error = cs40l26_pm_enter(dev);
	if (error)
		return error;

	error = cl_dsp_get_reg(cs40l26->dsp, "TIMEOUT_MS",
			CL_DSP_XM_UNPACKED_TYPE, CS40L26_VIBEGEN_ALGO_ID, &reg);
	if (error)
		goto pm_err;

	error = regmap_write(regmap, reg, 0);
	if (error) {
		dev_err(dev, "Failed to set TIMEOUT_MS\n");
		goto pm_err;
	}

	error = cs40l26_logger_setup(cs40l26);
	if (error)
		goto pm_err;

	error = cs40l26_asp_config(cs40l26);
	if (error)
		goto pm_err;

	if (cs40l26->revid == CS40L26_REVID_B2) {
		error = cs40l26_mailbox_write(cs40l26, CS40L26_DSP_MBOX_CMD_OWT_RESET);
		if (error)
			goto pm_err;
	}

	dev_info(dev, "%s loaded with %d RAM waveforms (%d from cs40l26.bin + %d from OWT)\n",
			CS40L26_DEV_NAME, cs40l26_num_waves(cs40l26),
			cs40l26_num_ram_waves(cs40l26), cs40l26_num_owt_waves(cs40l26));

	value = (cs40l26->comp_enable_redc << CS40L26_COMP_EN_REDC_SHIFT) |
			(cs40l26->comp_enable_f0 << CS40L26_COMP_EN_F0_SHIFT);

	if (cs40l26->fw_id != CS40L26_FW_CALIB_ID) {
		error = cl_dsp_get_reg(cs40l26->dsp, "COMPENSATION_ENABLE", CL_DSP_XM_UNPACKED_TYPE,
				CS40L26_VIBEGEN_ALGO_ID, &reg);
		if (error)
			goto pm_err;

		error = regmap_write(cs40l26->regmap, reg, value);
		if (error)
			dev_err(dev, "Failed to configure compensation\n");
	}

pm_err:
	cs40l26_pm_exit(dev);

	return error;
}

static void cs40l26_gain_adjust(struct cs40l26_private *cs40l26, s32 adjust)
{
	u16 total, asp, change;

	if (abs(adjust) > 100) {
		dev_warn(cs40l26->dev, "Gain adjust %d invalid, not applied\n", adjust);
		return;
	}

	asp = cs40l26->asp_scale_pct;

	if (adjust < 0) {
		change = (u16) ((adjust * -1) & 0xFFFF);
		if (asp < change)
			total = 0;
		else
			total = asp - change;
	} else {
		change = (u16) (adjust & 0xFFFF);
		total = asp + change;
		if (total > CS40L26_GAIN_FULL_SCALE)
			total = CS40L26_GAIN_FULL_SCALE;
	}

	cs40l26->asp_scale_pct = total;
}

int cs40l26_svc_le_estimate(struct cs40l26_private *cs40l26, unsigned int *le)
{
	struct device *dev = cs40l26->dev;
	unsigned int reg, le_est = 0;
	int error, i;

	error = cs40l26_mailbox_write(cs40l26, CS40L26_DSP_MBOX_CMD_LE_EST);
	if (error)
		return error;

	error = cl_dsp_get_reg(cs40l26->dsp, "LE_EST_STATUS", CL_DSP_YM_UNPACKED_TYPE,
			CS40L26_SVC_ALGO_ID, &reg);
	if (error)
		return error;

	for (i = 0; i < CS40L26_SVC_LE_MAX_ATTEMPTS; i++) {
		usleep_range(CS40L26_SVC_LE_EST_TIME_US, CS40L26_SVC_LE_EST_TIME_US + 100);
		error = regmap_read(cs40l26->regmap, reg, &le_est);
		if (error) {
			dev_err(dev, "Failed to get LE_EST_STATUS\n");
			return error;
		}

		dev_info(dev, "Measured Le Estimation = %u\n", le_est);

		if (le_est)
			break;
	}

	*le = le_est;

	return 0;
}
EXPORT_SYMBOL_GPL(cs40l26_svc_le_estimate);

static void cs40l26_tuning_select_from_svc_le(struct cs40l26_private *cs40l26,
		unsigned int le, u32 *tuning_num)
{
	int i;

	if (le) {
		for (i = 0; i < cs40l26->num_svc_le_vals; i++) {
			if (le >= cs40l26->svc_le_vals[i]->min &&
					le <= cs40l26->svc_le_vals[i]->max) {
				*tuning_num = cs40l26->svc_le_vals[i]->n;

				cs40l26_gain_adjust(cs40l26, cs40l26->svc_le_vals[i]->gain_adjust);
				break;
			}
		}
	}

	if (!le || i == cs40l26->num_svc_le_vals)
		dev_warn(cs40l26->dev, "Using default tunings\n");
}

static char **cs40l26_get_tuning_names(struct cs40l26_private *cs40l26, int *actual_num_files,
		u32 tuning)
{
	int i, file_count = 0;
	char **coeff_files;

	coeff_files = kcalloc(CS40L26_MAX_TUNING_FILES, sizeof(char *), GFP_KERNEL);
	if (!coeff_files)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < CS40L26_MAX_TUNING_FILES; i++) {
		coeff_files[i] = kzalloc(CS40L26_FILE_NAME_MAX_LEN, GFP_KERNEL);
		if (!coeff_files[i])
			goto err_free;
	}

	if (tuning) {
		snprintf(coeff_files[file_count++], CS40L26_FILE_NAME_MAX_LEN, "%s%d%s",
				CS40L26_WT_FILE_PREFIX, tuning, CS40L26_FILE_SUFFIX);
		snprintf(coeff_files[file_count++], CS40L26_FILE_NAME_MAX_LEN, "%s%d%s",
				CS40L26_SVC_FILE_PREFIX, tuning, CS40L26_FILE_SUFFIX);
	} else {
		strscpy(coeff_files[file_count++], CS40L26_WT_FILE_NAME,
				CS40L26_FILE_NAME_MAX_LEN);
		strscpy(coeff_files[file_count++], CS40L26_SVC_FILE_NAME,
				CS40L26_FILE_NAME_MAX_LEN);
	}

	if (cl_dsp_algo_is_present(cs40l26->dsp, CS40L26_LF0T_ALGO_ID))
		strscpy(coeff_files[file_count++], CS40L26_LF0T_FILE_NAME,
				CS40L26_FILE_NAME_MAX_LEN);

	if (cl_dsp_algo_is_present(cs40l26->dsp, CS40L26_DVL_ALGO_ID))
		strscpy(coeff_files[file_count++], CS40L26_DVL_FILE_NAME,
				CS40L26_FILE_NAME_MAX_LEN);

	if (cs40l26->fw_id == CS40L26_FW_ID) {
		if (cl_dsp_algo_is_present(cs40l26->dsp, CS40L26_A2H_ALGO_ID))
			strscpy(coeff_files[file_count++], CS40L26_A2H_FILE_NAME,
					CS40L26_FILE_NAME_MAX_LEN);

		if (cl_dsp_algo_is_present(cs40l26->dsp, CS40L26_EP_ALGO_ID))
			strscpy(coeff_files[file_count++], CS40L26_EP_FILE_NAME,
					CS40L26_FILE_NAME_MAX_LEN);

#if !defined(FORCE_DISABLE_DBC)
		strscpy(coeff_files[file_count++], CS40L26_DBC_FILE_NAME,
				CS40L26_FILE_NAME_MAX_LEN);
#endif
	} else {
		strscpy(coeff_files[file_count++], CS40L26_CALIB_FILE_NAME,
				CS40L26_FILE_NAME_MAX_LEN);

		if (cl_dsp_algo_is_present(cs40l26->dsp, CS40L26_LS_ALGO_ID))
			strscpy(coeff_files[file_count++], CS40L26_LS_CAL_FILE_NAME,
					CS40L26_FILE_NAME_MAX_LEN);
	}

	*actual_num_files = file_count;
	return coeff_files;

err_free:
	for (; i >= 0; i--)
		kfree(coeff_files[i]);
	kfree(coeff_files);
	*actual_num_files = 0;
	return ERR_PTR(-ENOMEM);
}

static int cs40l26_coeff_load(struct cs40l26_private *cs40l26, u32 tuning)
{
	struct device *dev = cs40l26->dev;
	int i, error, num_files_to_load;
	const struct firmware *coeff;
	char **coeff_files;

	coeff_files = cs40l26_get_tuning_names(cs40l26, &num_files_to_load, tuning);
	if (IS_ERR(coeff_files))
		return PTR_ERR(coeff_files);

	for (i = 0; i < num_files_to_load; i++) {
		error = request_firmware(&coeff, coeff_files[i], dev);
		if (error) {
			dev_warn(dev, "Continuing...\n");
			continue;
		}

		error = cl_dsp_coeff_file_parse(cs40l26->dsp, coeff);
		if (error) {
			dev_warn(dev, "Failed to load %s, %d. Continuing...\n", coeff_files[i],
					error);
		} else {
			dev_info(dev, "%s Loaded Successfully\n", coeff_files[i]);
			if (!strncmp(coeff_files[i], CS40L26_DBC_FILE_NAME,
					CS40L26_FILE_NAME_MAX_LEN))
				cs40l26->dbc_tuning_loaded = true;
		}

		release_firmware(coeff);
	}

	for (i = 0; i < CS40L26_MAX_TUNING_FILES; i++)
		kfree(coeff_files[i]);

	kfree(coeff_files);

	return 0;
}

static int cs40l26_change_fw_control_defaults(struct cs40l26_private *cs40l26)
{
	int error;

	error = cs40l26_pm_timeout_ms_set(cs40l26, CS40L26_DSP_STATE_STANDBY,
			cs40l26->pm_stdby_timeout_ms);
	if (error)
		return error;

	return cs40l26_pm_timeout_ms_set(cs40l26, CS40L26_DSP_STATE_ACTIVE,
			cs40l26->pm_active_timeout_ms);
}

static int cs40l26_get_fw_params(struct cs40l26_private *cs40l26)
{
	u32 id, min_rev, rev, branch;
	int error, maj, min, patch;

	error = cl_dsp_fw_rev_get(cs40l26->dsp, &rev);
	if (error)
		return error;

	branch = CL_DSP_GET_MAJOR(rev);
	maj = (int) branch;
	min = (int) CL_DSP_GET_MINOR(rev);
	patch = (int) CL_DSP_GET_PATCH(rev);

	error = cl_dsp_fw_id_get(cs40l26->dsp, &id);
	if (error)
		return error;

	switch (id) {
	case CS40L26_FW_ID:
		switch (branch) {
		case CS40L26_FW_BRANCH:
			min_rev = CS40L26_FW_MIN_REV;
			cs40l26->vibe_state_reporting = true;
			break;
		case CS40L26_FW_MAINT_BRANCH:
			min_rev = CS40L26_FW_MAINT_MIN_REV;
			cs40l26->vibe_state_reporting = false;
			break;
		case CS40L26_FW_B2_BRANCH:
			min_rev = CS40L26_FW_B2_MIN_REV;
			cs40l26->vibe_state_reporting = true;
			break;
		default:
			error = -EINVAL;
			break;
		}
		break;
	case CS40L26_FW_CALIB_ID:
		switch (branch) {
		case CS40L26_FW_CALIB_BRANCH:
			min_rev = CS40L26_FW_CALIB_MIN_REV;
			cs40l26->vibe_state_reporting = true;
			break;
		case CS40L26_FW_MAINT_CALIB_BRANCH:
			min_rev = CS40L26_FW_MAINT_CALIB_MIN_REV;
			cs40l26->vibe_state_reporting = false;
			break;
		case CS40L26_FW_B2_CALIB_BRANCH:
			min_rev = CS40L26_FW_B2_CALIB_MIN_REV;
			cs40l26->vibe_state_reporting = true;
			break;
		default:
			error = -EINVAL;
			break;
		}
		break;
	default:
		dev_err(cs40l26->dev, "Invalid FW ID: 0x%06X\n", id);
		return -EINVAL;
	}

	if (error) {
		dev_err(cs40l26->dev, "Rev. Branch 0x%02X invalid\n", maj);
		return error;
	}

	if (rev < min_rev) {
		dev_err(cs40l26->dev, "Invalid firmware revision: %d.%d.%d\n",
				maj, min, patch);
		return -EINVAL;
	}

	cs40l26->fw_id = id;

	dev_info(cs40l26->dev, "Firmware revision %d.%d.%d\n", maj, min, patch);

	return 0;
}

static int cs40l26_cl_dsp_reinit(struct cs40l26_private *cs40l26)
{
	int error;

	if (cs40l26->dsp) {
		error = cl_dsp_destroy(cs40l26->dsp);
		if (error) {
			dev_err(cs40l26->dev, "Failed to destroy DSP struct\n");
			return error;
		}

		cs40l26->dsp = NULL;
	}

	cs40l26->dsp = cl_dsp_create(cs40l26->dev, cs40l26->regmap);
	if (IS_ERR(cs40l26->dsp))
		return PTR_ERR(cs40l26->dsp);

	return cl_dsp_wavetable_create(cs40l26->dsp, CS40L26_VIBEGEN_ALGO_ID,
			CS40L26_WT_NAME_XM, CS40L26_WT_NAME_YM, CS40L26_WT_FILE_NAME);
}

static int cs40l26_fw_upload(struct cs40l26_private *cs40l26)
{
	bool svc_le_required = cs40l26->num_svc_le_vals && !cs40l26->calib_fw;
	struct device *dev = cs40l26->dev;
	u32 rev, branch, tuning_num = 0;
	unsigned int le = 0;
	const struct firmware *fw;
	int error;

	cs40l26->fw_loaded = false;

	error = cs40l26_cl_dsp_reinit(cs40l26);
	if (error)
		return error;

	if (cs40l26->calib_fw)
		error = request_firmware(&fw, CS40L26_FW_CALIB_NAME, dev);
	else
		error = request_firmware(&fw, CS40L26_FW_FILE_NAME, dev);

	if (error) {
		release_firmware(fw);
		return error;
	}

	if (!cs40l26->fw_rom_only) {
		error = cs40l26_dsp_pre_config(cs40l26);
		if (error)
			return error;
	}

	error = cl_dsp_firmware_parse(cs40l26->dsp, fw, !cs40l26->fw_rom_only);
	release_firmware(fw);
	if (error)
		return error;

	error = cs40l26_change_fw_control_defaults(cs40l26);
	if (error)
		return error;

	error = cs40l26_get_fw_params(cs40l26);
	if (error)
		return error;

	if (svc_le_required) {
		error = cl_dsp_fw_rev_get(cs40l26->dsp, &rev);
		if (error)
			return error;

		branch = CL_DSP_GET_MAJOR(rev);

		switch (branch) {
		case CS40L26_FW_MAINT_BRANCH:
			error = cs40l26_dsp_config(cs40l26);
			if (error)
				return error;

			error = cs40l26_pm_enter(dev);
			if (error)
				return error;

			error = cs40l26_svc_le_estimate(cs40l26, &le);
			if (error)
				dev_warn(dev, "svc_le_est failed: %d", error);

			cs40l26_pm_exit(dev);

			cs40l26_pm_runtime_teardown(cs40l26);

			error = cs40l26_dsp_pre_config(cs40l26);
			if (error)
				return error;

			break;
		case CS40L26_FW_BRANCH:
			le = cs40l26->svc_le_est_stored;
			break;
		default:
			dev_err(dev, "Invalid firmware branch, %d", branch);
			return -EINVAL;
		}

		cs40l26_tuning_select_from_svc_le(cs40l26, le, &tuning_num);
	}

	error = cs40l26_coeff_load(cs40l26, tuning_num);
	if (error)
		return error;

	return cs40l26_dsp_config(cs40l26);
}

static int cs40l26_request_irq(struct cs40l26_private *cs40l26)
{
	int error, irq, i;

	cs40l26_regmap_irq_chip.irq_drv_data = cs40l26;

	error = devm_regmap_add_irq_chip(cs40l26->dev, cs40l26->regmap,
			cs40l26->irq, IRQF_ONESHOT | IRQF_SHARED | IRQF_TRIGGER_LOW,
			-1, &cs40l26_regmap_irq_chip, &cs40l26->irq_data);
	if (error < 0) {
		dev_err(cs40l26->dev, "Failed to request threaded IRQ: %d\n", error);
		return error;
	}

	for (i = 0; i < ARRAY_SIZE(cs40l26_irqs); i++) {
		irq = regmap_irq_get_virq(cs40l26->irq_data, cs40l26_irqs[i].irq);
		if (irq < 0) {
			dev_err(cs40l26->dev, "Failed to get %s\n", cs40l26_irqs[i].name);
			return irq;
		}

		error = devm_request_threaded_irq(cs40l26->dev, irq, NULL, cs40l26_irqs[i].handler,
				IRQF_ONESHOT | IRQF_SHARED | IRQF_TRIGGER_LOW,
				cs40l26_irqs[i].name, cs40l26);
		if (error) {
			dev_err(cs40l26->dev, "Failed to request IRQ %s: %d\n",
					cs40l26_irqs[i].name, error);
			return error;
		}
	}

	return error;
}

int cs40l26_fw_swap(struct cs40l26_private *cs40l26, const u32 id)
{
	struct device *dev = cs40l26->dev;
	bool re_enable = false;
	int error, i, pseq_bytes_after_end;

	if (cs40l26->fw_loaded) {
		disable_irq(cs40l26->irq);
		cs40l26_pm_runtime_teardown(cs40l26);
		re_enable = true;
	}

	switch (cs40l26->revid) {
	case CS40L26_REVID_A1:
	case CS40L26_REVID_B0:
	case CS40L26_REVID_B1:
	case CS40L26_REVID_B2:
		break;
	default:
		dev_err(dev, "Unrecognized revid: 0x%02X\n", cs40l26->revid);
		return -EINVAL;
	}

	/* Clear write sequence memory following the end of script operation */
	pseq_bytes_after_end = CS40L26_PSEQ_MAX_BYTES -
			(cs40l26->rom_regs->rom_pseq_end_of_script -
			cs40l26->pseq.base_addr);
	for (i = 0; i < pseq_bytes_after_end / CL_DSP_BYTES_PER_WORD; i++) {
		error = cs40l26_dsp_write(cs40l26,
				cs40l26->rom_regs->rom_pseq_end_of_script + (i * 4), 0);
		if (error)
			return error;
	}

	/* reset pseq END_OF_SCRIPT to location from ROM */
	error = cs40l26_dsp_write(cs40l26, cs40l26->rom_regs->rom_pseq_end_of_script,
			CS40L26_WSEQ_OP_END << CS40L26_WSEQ_OP_SHIFT);
	if (error) {
		dev_err(dev, "Failed to reset pseq END_OF_SCRIPT %d\n", error);
		return error;
	}

	cs40l26_wseq_clear(cs40l26, &cs40l26->aseq);
	cs40l26_wseq_clear(cs40l26, &cs40l26->pseq);

	if (id == CS40L26_FW_CALIB_ID)
		cs40l26->calib_fw = true;
	else
		cs40l26->calib_fw = false;

	error = cs40l26_fw_upload(cs40l26);
	if (error)
		return error;

	if (cs40l26->fw_defer && cs40l26->fw_loaded) {
		error = cs40l26_request_irq(cs40l26);
		if (error)
			return error;

		cs40l26->fw_defer = false;
	}

	if (re_enable)
		enable_irq(cs40l26->irq);

	return error;
}
EXPORT_SYMBOL_GPL(cs40l26_fw_swap);

int cs40l26_wt_swap(struct cs40l26_private *cs40l26)
{
	struct device *dev = cs40l26->dev;
	u32 active_timeout, stdby_timeout;
	const struct firmware *wt;
	char *wt_file_name;
	u8 dsp_state;
	int error, i;

	if (!list_empty(&cs40l26->effect_head)) {
		dev_err(dev, "All uploaded effects must be removed before swapping wavetable\n");
		return -EPERM;
	}

	error = cs40l26_pm_state_transition(cs40l26, CS40L26_PM_STATE_PREVENT_HIBERNATE);
	if (error)
		return error;

	error = cs40l26_erase_gpi_mapping(cs40l26, CS40L26_GPIO_MAP_A_PRESS);
	if (error)
		goto gpio_restore;

	error = cs40l26_erase_gpi_mapping(cs40l26, CS40L26_GPIO_MAP_A_RELEASE);
	if (error)
		goto gpio_restore;

	error = cs40l26_pm_timeout_ms_get(cs40l26, CS40L26_DSP_STATE_ACTIVE, &active_timeout);
	if (error)
		goto gpio_restore;

	error = cs40l26_pm_timeout_ms_get(cs40l26, CS40L26_DSP_STATE_STANDBY, &stdby_timeout);
	if (error)
		goto gpio_restore;

	/* Set timeouts to minimum values for quick transition to shutdown */
	error = cs40l26_pm_timeout_ms_set(cs40l26, CS40L26_DSP_STATE_ACTIVE,
			CS40L26_PM_ACTIVE_TIMEOUT_MS_MIN);
	if (error)
		goto gpio_restore;

	error = cs40l26_pm_timeout_ms_set(cs40l26, CS40L26_DSP_STATE_STANDBY,
			CS40L26_PM_STDBY_TIMEOUT_MS_MIN);
	if (error)
		goto timeout_restore;

	error = cs40l26_mailbox_write(cs40l26, CS40L26_STOP_PLAYBACK);
	if (error)
		goto timeout_restore;

	error = cs40l26_mailbox_write(cs40l26, CS40L26_DSP_MBOX_CMD_STOP_I2S);
	if (error)
		goto timeout_restore;

	error = cs40l26_pm_state_transition(cs40l26, CS40L26_PM_STATE_SHUTDOWN);
	if (error)
		goto timeout_restore;

	for (i = 0; i < CS40L26_DSP_TIMEOUT_COUNT; i++) {
		error = cs40l26_dsp_state_get(cs40l26, &dsp_state);
		if (error)
			goto wake;

		if (dsp_state == CS40L26_DSP_STATE_SHUTDOWN)
			break;

		usleep_range(CS40L26_PM_STDBY_TIMEOUT_US_MIN,
				CS40L26_PM_STDBY_TIMEOUT_US_MIN + 100);
	}
	if (i >= CS40L26_DSP_TIMEOUT_COUNT) {
		dev_err(dev, "Timed out waiting for DSP shutdown\n");
		error = -ETIMEDOUT;
		goto wake;
	}

	wt_file_name = kzalloc(CS40L26_FILE_NAME_MAX_LEN, GFP_KERNEL);
	if (!wt_file_name) {
		error = -ENOMEM;
		goto wake;
	}

	if (cs40l26->wt_num)
		snprintf(wt_file_name, CS40L26_FILE_NAME_MAX_LEN, "%s%d%s",
				CS40L26_WT_FILE_PREFIX, cs40l26->wt_num, CS40L26_FILE_SUFFIX);
	else
		strscpy(wt_file_name, CS40L26_WT_FILE_NAME, CS40L26_FILE_NAME_MAX_LEN);

	error = request_firmware(&wt, wt_file_name, cs40l26->dev);
	if (error) {
		dev_err(cs40l26->dev, "Failed to request wavetable\n");
		goto free;
	}

	error = cl_dsp_coeff_file_parse(cs40l26->dsp, wt);
	release_firmware(wt);
	if (error)
		goto free;

	dev_info(dev, "%s Loaded Successfully\n", wt_file_name);

free:
	kfree(wt_file_name);

wake:
	cs40l26_pm_state_transition(cs40l26, CS40L26_PM_STATE_WAKEUP);

	cs40l26_pm_state_transition(cs40l26, CS40L26_PM_STATE_PREVENT_HIBERNATE);

timeout_restore:
	cs40l26_pm_timeout_ms_set(cs40l26, CS40L26_DSP_STATE_ACTIVE, active_timeout);

	cs40l26_pm_timeout_ms_set(cs40l26, CS40L26_DSP_STATE_STANDBY, stdby_timeout);

gpio_restore:
	cs40l26_set_gpio_from_dt(cs40l26);

	return error;
}
EXPORT_SYMBOL_GPL(cs40l26_wt_swap);

static int cs40l26_handle_svc_le_nodes(struct cs40l26_private *cs40l26)
{
	int i, error, init_count, node_count = 0;
	struct device *dev = cs40l26->dev;
	unsigned int min, max, index;
	struct fwnode_handle *child;
	const char *node_name;
	u32 gain_adjust_raw;
	s32 gain_adjust;

	init_count = device_get_child_node_count(dev);
	if (!init_count)
		return 0;

	cs40l26->svc_le_vals = devm_kcalloc(dev, init_count, sizeof(struct cs40l26_svc_le *),
			GFP_KERNEL);

	if (!cs40l26->svc_le_vals)
		return -ENOMEM;

	device_for_each_child_node(dev, child) {
		node_name = fwnode_get_name(child);

		if (strncmp(node_name, CS40L26_SVC_DT_PREFIX, 6))
			continue;

		if (fwnode_property_read_u32(child, "cirrus,min", &min)) {
			dev_err(dev, "No minimum value for SVC LE node\n");
			continue;
		}

		if (fwnode_property_read_u32(child, "cirrus,max", &max)) {
			dev_err(dev, "No maximum value for SVC LE node\n");
			continue;
		}

		if (max <= min) {
			dev_err(dev, "Max <= Min, SVC LE node malformed\n");
			continue;
		}

		if (fwnode_property_read_u32(child, "cirrus,gain-adjust", &gain_adjust_raw))
			gain_adjust = 0;
		else
			gain_adjust = (s32) gain_adjust_raw;

		if (fwnode_property_read_u32(child, "cirrus,index", &index)) {
			dev_err(dev, "No index specified for SVC LE node\n");
			continue;
		}

		for (i = 0; i < node_count; i++) {
			if (index == cs40l26->svc_le_vals[i]->n)
				break;
		}

		if (i < node_count) {
			dev_err(dev, "SVC LE nodes must have unique index\n");
			return -EINVAL;
		}

		cs40l26->svc_le_vals[node_count] = devm_kzalloc(dev, sizeof(struct cs40l26_svc_le),
				GFP_KERNEL);

		if (!cs40l26->svc_le_vals[node_count]) {
			error = -ENOMEM;
			goto err;
		}

		cs40l26->svc_le_vals[node_count]->min = min;
		cs40l26->svc_le_vals[node_count]->max = max;
		cs40l26->svc_le_vals[node_count]->gain_adjust = gain_adjust;
		cs40l26->svc_le_vals[node_count]->n = index;
		node_count++;
	}

	if (node_count != init_count)
		dev_warn(dev, "%d platform nodes unused for SVC LE\n", init_count - node_count);

	return node_count;

err:
	devm_kfree(dev, cs40l26->svc_le_vals);
	return error;
}

static int cs40l26_no_wait_ram_indices_get(struct cs40l26_private *cs40l26)
{
	int i, num, error;

	num = device_property_count_u32(cs40l26->dev, "cirrus,no-wait-ram-indices");
	if (num <= 0)
		return 0;

	cs40l26->no_wait_ram_indices = devm_kcalloc(cs40l26->dev, num, sizeof(u32), GFP_KERNEL);
	if (!cs40l26->no_wait_ram_indices)
		return -ENOMEM;

	error = device_property_read_u32_array(cs40l26->dev, "cirrus,no-wait-ram-indices",
			cs40l26->no_wait_ram_indices, num);
	if (error)
		goto err_free;

	for (i = 0; i < num; i++)
		cs40l26->no_wait_ram_indices[i] += CS40L26_RAM_INDEX_START;

	cs40l26->num_no_wait_ram_indices = num;

	return 0;

err_free:
	devm_kfree(cs40l26->dev, cs40l26->no_wait_ram_indices);
	cs40l26->num_no_wait_ram_indices = 0;
	return error;
}

static void cs40l26_hibernate_timer_callback(struct timer_list *t)
{
	struct cs40l26_private *cs40l26 = from_timer(cs40l26, t, hibernate_timer);

	dev_dbg(cs40l26->dev, "Time since ALLOW_HIBERNATE exceeded HE_TIME max");
}

static inline bool cs40l26_brwnout_is_valid(enum cs40l26_brwnout_type type, u32 val)
{
	if (type >= CS40L26_NUM_BRWNOUT_TYPES)
		return false;

	return (val <= cs40l26_brwnout_params[type].max) &&
			(val >= cs40l26_brwnout_params[type].min);
}

static void cs40l26_parse_brwnout_properties(struct cs40l26_private *cs40l26)
{
	struct device *dev = cs40l26->dev;
	int error;

	if (device_property_present(dev, "cirrus,vbbr-enable")) {
		cs40l26->vbbr.enable = true;

		error = device_property_read_u32(dev, "cirrus,vbbr-thld-uv",
				&cs40l26->vbbr.thld_uv);
		if (error || !cs40l26_brwnout_is_valid(CS40L26_VBBR_THLD, cs40l26->vbbr.thld_uv))
			cs40l26->vbbr.thld_uv = CS40L26_VBBR_THLD_UV_DEFAULT;

		error = device_property_read_u32(dev, "cirrus,vbbr-max-att-db",
						&cs40l26->vbbr.max_att_db);
		if (error || !cs40l26_brwnout_is_valid(CS40L26_VXBR_MAX_ATT,
							cs40l26->vbbr.max_att_db))
			cs40l26->vbbr.max_att_db = CS40L26_VXBR_MAX_ATT_DEFAULT;

		error = device_property_read_u32(dev, "cirrus,vbbr-atk-step",
						&cs40l26->vbbr.atk_step);
		if (error || !cs40l26_brwnout_is_valid(CS40L26_VXBR_ATK_STEP,
				cs40l26->vbbr.atk_step))
			cs40l26->vbbr.atk_step = CS40L26_VXBR_ATK_STEP_DEFAULT;

		error = device_property_read_u32(dev, "cirrus,vbbr-atk-rate",
						&cs40l26->vbbr.atk_rate);
		if (error || !cs40l26_brwnout_is_valid(CS40L26_VXBR_ATK_RATE,
				cs40l26->vbbr.atk_rate))
			cs40l26->vbbr.atk_rate = CS40L26_VXBR_ATK_RATE_DEFAULT;

		error = device_property_read_u32(dev, "cirrus,vbbr-wait", &cs40l26->vbbr.wait);
		if (error || !cs40l26_brwnout_is_valid(CS40L26_VXBR_WAIT, cs40l26->vbbr.wait))
			cs40l26->vbbr.wait = CS40L26_VXBR_WAIT_DEFAULT;

		error = device_property_read_u32(dev, "cirrus,vbbr-rel-rate",
						&cs40l26->vbbr.rel_rate);
		if (error || !cs40l26_brwnout_is_valid(CS40L26_VXBR_REL_RATE,
				cs40l26->vbbr.rel_rate))
			cs40l26->vbbr.rel_rate = CS40L26_VXBR_REL_RATE_DEFAULT;
	}

	if (device_property_present(dev, "cirrus,vpbr-enable")) {
		cs40l26->vpbr.enable = true;

		error = device_property_read_u32(dev, "cirrus,vpbr-thld-uv",
				&cs40l26->vpbr.thld_uv);
		if (error || !cs40l26_brwnout_is_valid(CS40L26_VPBR_THLD, cs40l26->vpbr.thld_uv))
			cs40l26->vpbr.thld_uv = CS40L26_VPBR_THLD_UV_DEFAULT;

		error = device_property_read_u32(dev, "cirrus,vpbr-max-att-db",
						&cs40l26->vpbr.max_att_db);
		if (error || !cs40l26_brwnout_is_valid(CS40L26_VXBR_MAX_ATT,
							cs40l26->vpbr.max_att_db))
			cs40l26->vpbr.max_att_db = CS40L26_VXBR_MAX_ATT_DEFAULT;

		error = device_property_read_u32(dev, "cirrus,vpbr-atk-step",
						&cs40l26->vpbr.atk_step);
		if (error || !cs40l26_brwnout_is_valid(CS40L26_VXBR_ATK_STEP,
				cs40l26->vpbr.atk_step))
			cs40l26->vpbr.atk_step = CS40L26_VXBR_ATK_STEP_DEFAULT;

		error = device_property_read_u32(dev, "cirrus,vpbr-atk-rate",
						&cs40l26->vpbr.atk_rate);
		if (error || !cs40l26_brwnout_is_valid(CS40L26_VXBR_ATK_RATE,
				cs40l26->vpbr.atk_rate))
			cs40l26->vpbr.atk_rate = CS40L26_VXBR_ATK_RATE_DEFAULT;

		error = device_property_read_u32(dev, "cirrus,vpbr-wait", &cs40l26->vpbr.wait);
		if (error || !cs40l26_brwnout_is_valid(CS40L26_VXBR_WAIT, cs40l26->vpbr.wait))
			cs40l26->vpbr.wait = CS40L26_VXBR_WAIT_DEFAULT;

		error = device_property_read_u32(dev, "cirrus,vpbr-rel-rate",
						&cs40l26->vpbr.rel_rate);
		if (error || !cs40l26_brwnout_is_valid(CS40L26_VXBR_REL_RATE,
				cs40l26->vpbr.rel_rate))
			cs40l26->vpbr.rel_rate = CS40L26_VXBR_REL_RATE_DEFAULT;
	}

}

static void cs40l26_wd_parse_properties(struct cs40l26_private *cs40l26)
{
	int error;

	if (device_property_present(cs40l26->dev, "cirrus,dc-wd-enable"))
		cs40l26->dc_wd_enabled = true;
	else
		return;

	error = device_property_read_u32(cs40l26->dev, "cirrus,dc-wd-thld", &cs40l26->dc_wd_thld);
	if (error)
		cs40l26->dc_wd_thld = CS40L26_DCIN_WD_THLD_100P0PCT_FS;

	error = device_property_read_u32(cs40l26->dev, "cirrus,dc-wd-dur", &cs40l26->dc_wd_dur);
	if (error)
		cs40l26->dc_wd_dur = CS40L26_DCIN_WD_DUR_20_MS;

	cs40l26->dc_wd_mute = device_property_present(cs40l26->dev, "cirrus,dc-wd-mute");
}

static int cs40l26_parse_properties(struct cs40l26_private *cs40l26)
{
	struct device *dev = cs40l26->dev;
	int error;

	cs40l26->fw_defer = device_property_present(dev, "cirrus,fw-defer");

	cs40l26->fw_rom_only = device_property_present(dev, "cirrus,fw-rom-only");

	cs40l26->calib_fw = device_property_present(dev, "cirrus,calib-fw");

	cs40l26->expl_mode_enabled = !device_property_present(dev, "cirrus,bst-expl-mode-disable");

	cs40l26_parse_brwnout_properties(cs40l26);

	cs40l26->bst_dcm_en = device_property_present(dev, "cirrus,bst-dcm-en");

	cs40l26->ng_enable = device_property_present(dev, "cirrus,ng-enable");

	error = device_property_read_u32(dev, "cirrus,bst-ipk-microamp", &cs40l26->bst_ipk);
	if (error)
		cs40l26->bst_ipk = CS40L26_BST_IPK_UA_DEFAULT;

	error = device_property_read_u32(dev, "cirrus,bst-ctl-microvolt", &cs40l26->bst_ctl);
	if (error)
		cs40l26->bst_ctl = CS40L26_BST_UV_MAX;

	error = device_property_read_u32(dev, "cirrus,clip-lvl-microvolt", &cs40l26->clip_lvl);
	if (error)
		cs40l26->clip_lvl = CS40L26_CLIP_LVL_UV_MAX;

	error = device_property_read_u32(dev, "cirrus,pm-stdby-timeout-ms",
			&cs40l26->pm_stdby_timeout_ms);
	if (error)
		cs40l26->pm_stdby_timeout_ms = CS40L26_PM_STDBY_TIMEOUT_MS_MIN;

	error = device_property_read_u32(dev, "cirrus,pm-active-timeout-ms",
			&cs40l26->pm_active_timeout_ms);
	if (error)
		cs40l26->pm_active_timeout_ms = CS40L26_PM_ACTIVE_TIMEOUT_MS_DEFAULT;

	error = cs40l26_handle_svc_le_nodes(cs40l26);
	if (error < 0)
		cs40l26->num_svc_le_vals = 0;
	else
		cs40l26->num_svc_le_vals = error;

	error = device_property_read_u32(dev, "cirrus,asp-gain-scale-pct", &cs40l26->asp_scale_pct);
	if (error)
		cs40l26->asp_scale_pct = CS40L26_GAIN_FULL_SCALE;

	cs40l26->gain_pct = CS40L26_GAIN_FULL_SCALE;
	cs40l26->gain_tmp = CS40L26_GAIN_FULL_SCALE;

	error = device_property_read_u32(dev, "cirrus,ng-thld", &cs40l26->ng_thld);
	if (error)
		cs40l26->ng_thld = CS40L26_NG_THRESHOLD_DEFAULT;

	error = device_property_read_u32(dev, "cirrus,ng-delay", &cs40l26->ng_delay);
	if (error)
		cs40l26->ng_delay = CS40L26_NG_DELAY_DEFAULT;

	cs40l26->aux_ng_enable = device_property_present(dev, "cirrus,aux-ng-enable");

	error = device_property_read_u32(dev, "cirrus,aux-ng-thld", &cs40l26->aux_ng_thld);
	if (error)
		cs40l26->aux_ng_thld = CS40L26_AUX_NG_THLD_DEFAULT;

	error = device_property_read_u32(dev, "cirrus,aux-ng-delay", &cs40l26->aux_ng_delay);
	if (error)
		cs40l26->aux_ng_delay = CS40L26_AUX_NG_HOLD_DEFAULT;

	cs40l26_wd_parse_properties(cs40l26);

	error = device_property_read_u32(dev, "cirrus,f0-default", &cs40l26->f0_default);
	if (error && error != -EINVAL)
		return error;

	error = device_property_read_u32(dev, "cirrus,redc-default", &cs40l26->redc_default);
	if (error && error != -EINVAL)
		return error;

	error = device_property_read_u32(dev, "cirrus,q-default", &cs40l26->q_default);
	if (error && error != -EINVAL)
		return error;

	cs40l26->dbc_enable = device_property_present(dev, "cirrus,dbc-enable");

	error = device_property_read_u32(dev, "cirrus,dbc-env-rel-coef",
			&cs40l26->dbc_configs[CS40L26_DBC_ENV_REL_COEF]);
	if (error)
		cs40l26->dbc_configs[CS40L26_DBC_ENV_REL_COEF] = CS40L26_DBC_DEFAULT;

	error = device_property_read_u32(dev, "cirrus,dbc-fall-headroom",
			&cs40l26->dbc_configs[CS40L26_DBC_FALL_HEADROOM]);
	if (error)
		cs40l26->dbc_configs[CS40L26_DBC_FALL_HEADROOM] = CS40L26_DBC_DEFAULT;

	error = device_property_read_u32(dev, "cirrus,dbc-rise-headroom",
			&cs40l26->dbc_configs[CS40L26_DBC_RISE_HEADROOM]);
	if (error)
		cs40l26->dbc_configs[CS40L26_DBC_RISE_HEADROOM] = CS40L26_DBC_DEFAULT;

	error = device_property_read_u32(dev, "cirrus,dbc-tx-lvl-hold-off-ms",
			&cs40l26->dbc_configs[CS40L26_DBC_TX_LVL_HOLD_OFF_MS]);
	if (error)
		cs40l26->dbc_configs[CS40L26_DBC_TX_LVL_HOLD_OFF_MS] = CS40L26_DBC_DEFAULT;

	error = device_property_read_u32(dev, "cirrus,dbc-tx-lvl-thresh-fs",
			&cs40l26->dbc_configs[CS40L26_DBC_TX_LVL_THRESH_FS]);
	if (error)
		cs40l26->dbc_configs[CS40L26_DBC_TX_LVL_THRESH_FS] = CS40L26_DBC_DEFAULT;

	cs40l26->pwle_zero_cross = device_property_present(dev, "cirrus,pwle-zero-cross-en");

	if (device_property_present(dev, "cirrus,gpo-playback-monitor")) {
		cs40l26->gpo_playback_mon = true;
		cs40l26->press_idx = CS40L26_EVENT_MAP_GPI_DISABLE;
		cs40l26->release_idx = CS40L26_EVENT_MAP_GPI_DISABLE;
	}  else {
		cs40l26->gpo_playback_mon = false;
		cs40l26->press_idx = gpio_map_get(dev, CS40L26_GPIO_MAP_A_PRESS);
		cs40l26->release_idx = gpio_map_get(dev, CS40L26_GPIO_MAP_A_RELEASE);
	}

	return cs40l26_no_wait_ram_indices_get(cs40l26);
}

int cs40l26_probe(struct cs40l26_private *cs40l26)
{
	struct device *dev = cs40l26->dev;
	int error;

	mutex_init(&cs40l26->lock);

	cs40l26->vibe_workqueue = alloc_ordered_workqueue("vibe_workqueue", WQ_HIGHPRI);
	if (!cs40l26->vibe_workqueue) {
		error = -ENOMEM;
		goto err;
	}

	INIT_WORK(&cs40l26->vibe_start_work, cs40l26_vibe_start_worker);
	INIT_WORK(&cs40l26->vibe_stop_work, cs40l26_vibe_stop_worker);
	INIT_WORK(&cs40l26->set_gain_work, cs40l26_set_gain_worker);
	INIT_WORK(&cs40l26->upload_work, cs40l26_upload_worker);
	INIT_WORK(&cs40l26->erase_work, cs40l26_erase_worker);

	timer_setup(&cs40l26->hibernate_timer, cs40l26_hibernate_timer_callback, 0);

	error = devm_regulator_bulk_get(dev, CS40L26_NUM_SUPPLIES, cs40l26_supplies);
	if (error) {
		dev_err(dev, "Failed to request core supplies: %d\n", error);
		goto err;
	}

	error = cs40l26_parse_properties(cs40l26);
	if (error)
		goto err;

	error = regulator_bulk_enable(CS40L26_NUM_SUPPLIES, cs40l26_supplies);
	if  (error) {
		dev_err(dev, "Failed to enable core supplies\n");
		goto err;
	}

	cs40l26->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(cs40l26->reset_gpio)) {
		dev_err(dev, "Failed to get reset GPIO\n");

		error = PTR_ERR(cs40l26->reset_gpio);
		cs40l26->reset_gpio = NULL;
		goto err;
	}

	usleep_range(CS40L26_MIN_RESET_PULSE_WIDTH, CS40L26_MIN_RESET_PULSE_WIDTH + 100);

	gpiod_set_value_cansleep(cs40l26->reset_gpio, 0);

	usleep_range(CS40L26_CONTROL_PORT_READY_DELAY, CS40L26_CONTROL_PORT_READY_DELAY + 100);

	/*
	 * The DSP may lock up if a haptic effect is triggered via
	 * GPI event or control port and the PLL is set to closed-loop.
	 *
	 * Set PLL to open-loop and remove any default GPI mappings
	 * to prevent this while the driver is loading and configuring RAM
	 * firmware.
	 */

	error = cs40l26_set_pll_loop(cs40l26, CS40L26_PLL_REFCLK_SET_OPEN_LOOP);
	if (error)
		goto err;

	error = cs40l26_part_num_resolve(cs40l26);
	if (error)
		goto err;

	error = cs40l26_erase_gpi_mapping(cs40l26, CS40L26_GPIO_MAP_A_PRESS);
	if (error)
		goto err;

	error = cs40l26_erase_gpi_mapping(cs40l26, CS40L26_GPIO_MAP_A_RELEASE);
	if (error)
		goto err;

	/* Set LRA to high-z to avoid fault conditions */
	error = regmap_update_bits(cs40l26->regmap, CS40L26_TST_DAC_MSM_CONFIG,
			CS40L26_SPK_DEFAULT_HIZ_MASK, 1 << CS40L26_SPK_DEFAULT_HIZ_SHIFT);
	if (error) {
		dev_err(dev, "Failed to set LRA to HI-Z\n");
		goto err;
	}

	init_completion(&cs40l26->i2s_cont);
	init_completion(&cs40l26->erase_cont);
	init_completion(&cs40l26->cal_f0_cont);
	init_completion(&cs40l26->cal_redc_cont);
	init_completion(&cs40l26->cal_dvl_peq_cont);
	init_completion(&cs40l26->cal_ls_cont);

	if (!cs40l26->fw_defer) {
		error = cs40l26_fw_upload(cs40l26);
		if (error)
			goto err;

		error = cs40l26_request_irq(cs40l26);
		if (error)
			goto err;
	}

	error = cs40l26_input_init(cs40l26);
	if (error)
		goto err;

	INIT_LIST_HEAD(&cs40l26->effect_head);

	error = devm_mfd_add_devices(dev, PLATFORM_DEVID_AUTO, cs40l26_devs,
			CS40L26_NUM_MFD_DEVS, NULL, 0, NULL);
	if (error) {
		dev_err(dev, "Failed to register codec component\n");
		goto err;
	}

/*
 * Support boot-up viberation from RAM during probe stage.
 * Only waveform = FF_SINE is supported in this way, please do NOT use waveform = FF_CUSTOM
 */
#if defined(BOOTUP_VIBE_FROM_RAM)
	{
		struct ff_effect effect, effect_old;

		memset(&effect, 0, sizeof(effect));

		effect.id = 0;
		effect.type = FF_PERIODIC;
		effect.replay.length = 100; // Playback duration in ms
		effect.direction = 0;
		effect.u.periodic.waveform = FF_SINE;
		effect.u.periodic.period = 8; // Sine wave period in ms (5 ms = 200 Hz)
		effect.u.periodic.magnitude = 100; // Sine wave (BUZZGEN) level(range = 0 ~ 255)

		error = cs40l26_upload_effect(cs40l26->input, &effect, &effect_old);
		if (error) {
			dev_err(dev, "Failed to upload from probe.\n");
			//goto err;
		}

		cs40l26->input->ff->effects[0] = effect;

		error = cs40l26_playback_effect(cs40l26->input, 0, 1);
		if (error) {
			dev_err(dev, "Failed to playback from probe.\n");
			//goto err;
		}

		error = cs40l26_erase_effect(cs40l26->input, 0);
		if (error) {
			dev_err(dev, "Failed to erase from probe.\n");
			//goto err;
		}

		dev_dbg(dev, "Vibrate from probe done.\n");
	}
#endif

	return 0;
err:
	cs40l26_remove(cs40l26);

	return error;
}
EXPORT_SYMBOL_GPL(cs40l26_probe);

int cs40l26_remove(struct cs40l26_private *cs40l26)
{
	struct regulator *vp_consumer = cs40l26_supplies[CS40L26_VP_SUPPLY].consumer;
	struct regulator *va_consumer = cs40l26_supplies[CS40L26_VA_SUPPLY].consumer;

	disable_irq(cs40l26->irq);
	mutex_destroy(&cs40l26->lock);

	cs40l26_pm_runtime_teardown(cs40l26);

	if (cs40l26->vibe_workqueue) {
		cancel_work_sync(&cs40l26->vibe_start_work);
		cancel_work_sync(&cs40l26->vibe_stop_work);
		cancel_work_sync(&cs40l26->set_gain_work);
		cancel_work_sync(&cs40l26->upload_work);
		cancel_work_sync(&cs40l26->erase_work);
		destroy_workqueue(cs40l26->vibe_workqueue);
	}

	if (vp_consumer)
		regulator_disable(vp_consumer);

	if (va_consumer)
		regulator_disable(va_consumer);

	gpiod_set_value_cansleep(cs40l26->reset_gpio, 1);

	if (cs40l26->vibe_init_success)
		sysfs_remove_groups(&cs40l26->input->dev.kobj, cs40l26_attr_groups);

	cs40l26_wseq_clear(cs40l26, &cs40l26->aseq);
	cs40l26_wseq_clear(cs40l26, &cs40l26->pseq);

#ifdef CONFIG_DEBUG_FS
	cs40l26_debugfs_cleanup(cs40l26);
#endif

	if (cs40l26->input)
		input_unregister_device(cs40l26->input);

	return 0;
}
EXPORT_SYMBOL_GPL(cs40l26_remove);

int cs40l26_pm_enter(struct device *dev)
{
	int error;

	dev_dbg(dev, "%s\n", __func__);
	
	error = pm_runtime_get_sync(dev);
	if (error < 0) {
		cs40l26_resume_error_handle(dev, error);
		return error;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(cs40l26_pm_enter);

void cs40l26_pm_exit(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);
}
EXPORT_SYMBOL_GPL(cs40l26_pm_exit);

int cs40l26_suspend(struct device *dev)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);

	dev_dbg(cs40l26->dev, "%s: Enabling hibernation\n", __func__);

	return cs40l26_pm_state_transition(cs40l26, CS40L26_PM_STATE_ALLOW_HIBERNATE);
}
EXPORT_SYMBOL_GPL(cs40l26_suspend);

int cs40l26_sys_suspend(struct device *dev)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	struct i2c_client *i2c_client = to_i2c_client(dev);

	dev_dbg(cs40l26->dev, "System suspend, disabling IRQ\n");

	disable_irq(i2c_client->irq);

	return 0;
}
EXPORT_SYMBOL_GPL(cs40l26_sys_suspend);

int cs40l26_sys_suspend_noirq(struct device *dev)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	struct i2c_client *i2c_client = to_i2c_client(dev);

	dev_dbg(cs40l26->dev, "Late system suspend, re-enabling IRQ\n");
	enable_irq(i2c_client->irq);

	return 0;
}
EXPORT_SYMBOL_GPL(cs40l26_sys_suspend_noirq);

void cs40l26_resume_error_handle(struct device *dev, int error)
{
	dev_alert(dev, "PM Runtime Resume failed: %d\n", error);

	pm_runtime_set_active(dev);

	cs40l26_pm_exit(dev);
}
EXPORT_SYMBOL_GPL(cs40l26_resume_error_handle);

int cs40l26_resume(struct device *dev)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);

	dev_dbg(cs40l26->dev, "%s: Disabling hibernation\n", __func__);

	return cs40l26_pm_state_transition(cs40l26, CS40L26_PM_STATE_PREVENT_HIBERNATE);
}
EXPORT_SYMBOL_GPL(cs40l26_resume);

int cs40l26_sys_resume(struct device *dev)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	struct i2c_client *i2c_client = to_i2c_client(dev);

	dev_dbg(cs40l26->dev, "System resume, re-enabling IRQ\n");

	enable_irq(i2c_client->irq);

	return 0;
}
EXPORT_SYMBOL_GPL(cs40l26_sys_resume);

int cs40l26_sys_resume_noirq(struct device *dev)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	struct i2c_client *i2c_client = to_i2c_client(dev);

	dev_dbg(cs40l26->dev, "Early system resume, disabling IRQ\n");

	disable_irq(i2c_client->irq);

	return 0;
}
EXPORT_SYMBOL_GPL(cs40l26_sys_resume_noirq);

const struct dev_pm_ops cs40l26_pm_ops = {
	SET_RUNTIME_PM_OPS(cs40l26_suspend, cs40l26_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(cs40l26_sys_suspend, cs40l26_sys_resume)
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(cs40l26_sys_suspend_noirq, cs40l26_sys_resume_noirq)
};
EXPORT_SYMBOL_GPL(cs40l26_pm_ops);

MODULE_DESCRIPTION("CS40L26 Boosted Mono Class D Amplifier for Haptics");
MODULE_AUTHOR("Fred Treven, Cirrus Logic Inc. <fred.treven@cirrus.com>");
MODULE_LICENSE("GPL");
