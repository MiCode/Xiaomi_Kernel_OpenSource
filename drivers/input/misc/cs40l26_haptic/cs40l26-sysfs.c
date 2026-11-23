// SPDX-License-Identifier: GPL-2.0
//
// cs40l26-sysfs.c -- CS40L26 Boosted Haptic Driver with Integrated DSP and
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

#include <linux/mfd/cs40l26.h>

static const struct regmap_config cs40l26_broadcast_regmap = {
	.reg_bits =		32,
	.reg_stride =		4,
	.val_bits =		32,
	.reg_format_endian =	REGMAP_ENDIAN_BIG,
	.val_format_endian =	REGMAP_ENDIAN_BIG,
	.writeable_reg =	cs40l26_broadcast_writeable_reg,
	.readable_reg =		cs40l26_broadcast_readable_reg,
};

static ssize_t broadcast_master_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);

	if (!cs40l26->broadcast_addr) {
		dev_err(cs40l26->dev, "Broadcast I2C disabled on this device\n");
		return cs40l26_log_err(cs40l26, -EPERM, CS40L26_ERR_TYPE_HW, __func__);
	}

	return snprintf(buf, PAGE_SIZE, "%u\n", cs40l26->broadcast_client ? 1 : 0);
}

static ssize_t broadcast_master_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	struct i2c_client *client;
	int error = 0;
	u32 broadcast;

	if (!cs40l26->broadcast_addr) {
		dev_err(cs40l26->dev, "Broadcast I2C disabled on this device\n");
		return cs40l26_log_err(cs40l26, -EPERM, CS40L26_ERR_TYPE_HW, __func__);
	}

	error = kstrtou32(buf, 10, &broadcast);
	if (error)
		return error;

	if (broadcast == 0) {
		if (cs40l26->broadcast_regmap) {
			regmap_exit(cs40l26->broadcast_regmap);
			cs40l26->broadcast_regmap = NULL;
		}

		if (cs40l26->broadcast_client) {
			i2c_unregister_device(cs40l26->broadcast_client);
			cs40l26->broadcast_client = NULL;
		}
	} else if (broadcast == 1) {
		if (cs40l26->broadcast_client) {
			dev_err(cs40l26->dev, "Already broadcast master device\n");
			return cs40l26_log_err(cs40l26, -EINVAL, CS40L26_ERR_TYPE_HW, __func__);
		}

		client = of_find_i2c_device_by_node(cs40l26->dev->of_node);
		if (!client) {
			dev_err(cs40l26->dev, "Failed to get i2c client\n");
			return cs40l26_log_err(cs40l26, -ENODATA, CS40L26_ERR_TYPE_DT, __func__);
		}

		cs40l26->broadcast_client = i2c_new_dummy_device(client->adapter,
				cs40l26->broadcast_addr);
		if (IS_ERR(cs40l26->broadcast_client)) {
			dev_err(cs40l26->dev, "Unable to create broadcast client: %ld\n",
					PTR_ERR(cs40l26->broadcast_client));
			error = PTR_ERR(cs40l26->broadcast_client);
			cs40l26->broadcast_client = NULL;
			return cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_HW, __func__);
		}

		cs40l26->broadcast_regmap = regmap_init_i2c(cs40l26->broadcast_client,
				&cs40l26_broadcast_regmap);
		if (IS_ERR(cs40l26->broadcast_regmap)) {
			dev_err(cs40l26->dev, "Failed to allocate broadcast regmap: %ld\n",
					PTR_ERR(cs40l26->broadcast_regmap));
			error = PTR_ERR(cs40l26->broadcast_regmap);
			i2c_unregister_device(cs40l26->broadcast_client);
			cs40l26->broadcast_client = NULL;
			cs40l26->broadcast_regmap = NULL;
			return cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_HW, __func__);
		}
	} else {
		return -EINVAL;
	}

	return error ? error : count;
}
static DEVICE_ATTR_RW(broadcast_master);

static ssize_t dsp_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u8 dsp_state;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	error = cs40l26_dsp_state_get(cs40l26, &dsp_state);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;
	else
		return snprintf(buf, PAGE_SIZE, "%u\n", (unsigned int) (dsp_state & 0xFF));
}
static DEVICE_ATTR_RO(dsp_state);

static ssize_t owt_lib_compat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1.0.0\n");
}
static DEVICE_ATTR_RO(owt_lib_compat);

static ssize_t overprotection_gain_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 reg, op_gain;
	int error;

	if (!cl_dsp_algo_is_present(cs40l26->dsp, CS40L26_EP_ALGO_ID))
		return -EPERM;

	error = cl_dsp_get_reg(cs40l26->dsp, "PROTECTION_XM_OP_GAIN",
			CL_DSP_XM_UNPACKED_TYPE, CS40L26_EP_ALGO_ID, &reg);
	if (error)
		return error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	error = regmap_read(cs40l26->regmap, reg, &op_gain);

	cs40l26_pm_exit(cs40l26->dev);

	return error ? error : snprintf(buf, PAGE_SIZE, "%d\n", op_gain);
}

static ssize_t overprotection_gain_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 reg, op_gain;
	int error;

	if (!cl_dsp_algo_is_present(cs40l26->dsp, CS40L26_EP_ALGO_ID))
		return -EPERM;

	error = kstrtou32(buf, 10, &op_gain);
	if (error)
		return error;

	if (op_gain < CS40L26_OVERPROTECTION_GAIN_MIN || op_gain > CS40L26_OVERPROTECTION_GAIN_MAX)
		return -EINVAL;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	error = cl_dsp_get_reg(cs40l26->dsp, "PROTECTION_XM_OP_GAIN",
			CL_DSP_XM_UNPACKED_TYPE, CS40L26_EP_ALGO_ID, &reg);
	if (error)
		goto err_pm;

	error = regmap_write(cs40l26->regmap, reg, op_gain);

err_pm:
	cs40l26_pm_exit(cs40l26->dev);

	return error ? error : count;
}
static DEVICE_ATTR_RW(overprotection_gain);

static ssize_t halo_heartbeat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 reg, halo_heartbeat;
	int error;

	error = cl_dsp_get_reg(cs40l26->dsp, "HALO_HEARTBEAT", CL_DSP_XM_UNPACKED_TYPE,
			cs40l26->fw_id, &reg);
	if (error)
		return error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	error = regmap_read(cs40l26->regmap, reg, &halo_heartbeat);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;

	return snprintf(buf, PAGE_SIZE, "%d\n", halo_heartbeat);
}
static DEVICE_ATTR_RO(halo_heartbeat);

static ssize_t pm_stdby_timeout_ms_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 timeout_ms;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	error = cs40l26_pm_timeout_ms_get(cs40l26, CS40L26_DSP_STATE_STANDBY, &timeout_ms);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;

	return snprintf(buf, PAGE_SIZE, "%u\n", timeout_ms);
}

static ssize_t pm_stdby_timeout_ms_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 timeout_ms;
	int error;

	error = kstrtou32(buf, 10, &timeout_ms);
	if (error)
		return -EINVAL;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	error = cs40l26_pm_timeout_ms_set(cs40l26, CS40L26_DSP_STATE_STANDBY, timeout_ms);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;

	return count;
}
static DEVICE_ATTR_RW(pm_stdby_timeout_ms);

static ssize_t pm_active_timeout_ms_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 timeout_ms;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	error = cs40l26_pm_timeout_ms_get(cs40l26, CS40L26_DSP_STATE_ACTIVE, &timeout_ms);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;

	return snprintf(buf, PAGE_SIZE, "%u\n", timeout_ms);
}

static ssize_t pm_active_timeout_ms_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 timeout_ms;
	int error;

	error = kstrtou32(buf, 10, &timeout_ms);
	if (error)
		return -EINVAL;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	error = cs40l26_pm_timeout_ms_set(cs40l26, CS40L26_DSP_STATE_ACTIVE, timeout_ms);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;

	return count;
}
static DEVICE_ATTR_RW(pm_active_timeout_ms);

static ssize_t vibe_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	unsigned int state;

	if (!cs40l26->vibe_state_reporting)  {
		dev_err(cs40l26->dev, "vibe_state not supported\n");
		return cs40l26_log_err(cs40l26, -EPERM, CS40L26_ERR_TYPE_FW, __func__);
	}

	mutex_lock(&cs40l26->lock);
	state = cs40l26->vibe_state;
	mutex_unlock(&cs40l26->lock);

	return snprintf(buf, PAGE_SIZE, "%u\n", state);
}
static DEVICE_ATTR_RO(vibe_state);

static ssize_t owt_free_space_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 reg, words;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	error = cl_dsp_get_reg(cs40l26->dsp, "OWT_SIZE_XM",
			CL_DSP_XM_UNPACKED_TYPE, CS40L26_VIBEGEN_ALGO_ID, &reg);
	if (error)
		goto err_pm;

	error = regmap_read(cs40l26->regmap, reg, &words);
	if (error) {
		dev_err(cs40l26->dev, "Failed to get remaining OWT space\n");
		cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_CP, __func__);
		goto err_pm;
	}

	error = snprintf(buf, PAGE_SIZE, "%d\n", words * CL_DSP_BYTES_PER_WORD);

err_pm:
	cs40l26_pm_exit(cs40l26->dev);

	return error;
}
static DEVICE_ATTR_RO(owt_free_space);

static ssize_t die_temp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	struct regmap *regmap = cs40l26->regmap;
	u16 die_temp;
	int error;
	u32 val;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	error = regmap_read(regmap, CS40L26_GLOBAL_ENABLES, &val);
	if (error) {
		dev_err(cs40l26->dev, "Failed to read GLOBAL_EN status\n");
		cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_CP, __func__);
		goto err_pm;
	}

	if (!(val & CS40L26_GLOBAL_EN_MASK)) {
		dev_err(cs40l26->dev, "Global enable must be set to get die temp.\n");
		error = -EPERM;
		cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_HW, __func__);
		goto err_pm;
	}

	error = regmap_read(regmap, CS40L26_ENABLES_AND_CODES_DIG, &val);
	if (error) {
		dev_err(cs40l26->dev, "Failed to get die temperature\n");
		cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_CP, __func__);
		goto err_pm;
	}

	die_temp = (val & CS40L26_TEMP_RESULT_FILT_MASK) >> CS40L26_TEMP_RESULT_FILT_SHIFT;

	error = snprintf(buf, PAGE_SIZE, "0x%03X\n", die_temp);

err_pm:
	cs40l26_pm_exit(cs40l26->dev);

	return error;
}
static DEVICE_ATTR_RO(die_temp);

static ssize_t num_waves_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error, nwaves;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	nwaves = cs40l26_num_waves(cs40l26);
	if (nwaves < 0) {
		error = nwaves;
		goto err_pm;
	}

	error = snprintf(buf, PAGE_SIZE, "%d\n", nwaves);

err_pm:
	cs40l26_pm_exit(cs40l26->dev);

	return error;
}
static DEVICE_ATTR_RO(num_waves);

static ssize_t f0_offset_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	unsigned int reg, val;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "F0_OFFSET", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_VIBEGEN_ALGO_ID, &reg);
	if (error)
		goto err_mutex;


	error = regmap_read(cs40l26->regmap, reg, &val);
	if (error)
		goto err_mutex;

	error = snprintf(buf, PAGE_SIZE, "%u\n", val);

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error;
}

static ssize_t f0_offset_store(struct device *dev, struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	unsigned int reg, val;
	int error;

	error = kstrtou32(buf, 10, &val);
	if (error)
		return -EINVAL;

	if (val > CS40L26_F0_OFFSET_MAX && val < CS40L26_F0_OFFSET_MIN)
		return -EINVAL;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "F0_OFFSET", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_VIBEGEN_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_write(cs40l26->regmap, reg, val);
	if (error)
		goto err_mutex;

	error = count;

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error;
}
static DEVICE_ATTR_RW(f0_offset);

static ssize_t delay_before_stop_playback_us_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error;

	mutex_lock(&cs40l26->lock);

	error = snprintf(buf, PAGE_SIZE, "%d\n", cs40l26->delay_before_stop_playback_us);

	mutex_unlock(&cs40l26->lock);

	return error;
}

static ssize_t delay_before_stop_playback_us_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 val;
	int error;

	error = kstrtou32(buf, 10, &val);
	if (error)
		return -EINVAL;

	mutex_lock(&cs40l26->lock);

	cs40l26->delay_before_stop_playback_us = val;

	mutex_unlock(&cs40l26->lock);

	return count;
}
static DEVICE_ATTR_RW(delay_before_stop_playback_us);

static ssize_t f0_comp_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error;

	mutex_lock(&cs40l26->lock);

	if (cs40l26->fw_id == CS40L26_FW_CALIB_ID) {
		error = -EPERM;
		goto err_mutex;
	}

	if (cs40l26->comp_enable_pend) {
		error = -EIO;
		goto err_mutex;
	}

	error = snprintf(buf, PAGE_SIZE, "%d\n", cs40l26->comp_enable_f0);

err_mutex:
	mutex_unlock(&cs40l26->lock);

	return error;
}

static ssize_t f0_comp_enable_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error;
	unsigned int val;
	u32 reg, value;

	error = kstrtou32(buf, 10, &val);
	if (error)
		return -EINVAL;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	cs40l26->comp_enable_pend = true;
	cs40l26->comp_enable_f0 = val > 0;

	value = (cs40l26->comp_enable_redc << CS40L26_COMP_EN_REDC_SHIFT) |
			(cs40l26->comp_enable_f0 << CS40L26_COMP_EN_F0_SHIFT);

	if (cs40l26->fw_id == CS40L26_FW_CALIB_ID) {
		error = -EPERM;
	} else {
		error = cl_dsp_get_reg(cs40l26->dsp, "COMPENSATION_ENABLE", CL_DSP_XM_UNPACKED_TYPE,
				CS40L26_VIBEGEN_ALGO_ID, &reg);
		if (error)
			goto err_mutex;

		error = regmap_write(cs40l26->regmap, reg, value);
	}

	if (error)
		goto err_mutex;

	error = count;

err_mutex:
	cs40l26->comp_enable_pend = false;
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error;
}
static DEVICE_ATTR_RW(f0_comp_enable);

static ssize_t redc_comp_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error;

	mutex_lock(&cs40l26->lock);

	if (cs40l26->fw_id == CS40L26_FW_CALIB_ID) {
		error = -EPERM;
		goto err_mutex;
	}

	if (cs40l26->comp_enable_pend) {
		error = -EIO;
		goto err_mutex;
	}

	error = snprintf(buf, PAGE_SIZE, "%d\n", cs40l26->comp_enable_redc);

err_mutex:
	mutex_unlock(&cs40l26->lock);

	return error;
}

static ssize_t redc_comp_enable_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error;
	unsigned int val;
	u32 reg, value;

	error = kstrtou32(buf, 10, &val);
	if (error)
		return -EINVAL;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	cs40l26->comp_enable_pend = true;
	cs40l26->comp_enable_redc = val > 0;

	value = (cs40l26->comp_enable_redc << CS40L26_COMP_EN_REDC_SHIFT) |
			(cs40l26->comp_enable_f0 << CS40L26_COMP_EN_F0_SHIFT);

	if (cs40l26->fw_id == CS40L26_FW_CALIB_ID) {
		error = -EPERM;
	} else {
		error = cl_dsp_get_reg(cs40l26->dsp, "COMPENSATION_ENABLE", CL_DSP_XM_UNPACKED_TYPE,
				CS40L26_VIBEGEN_ALGO_ID, &reg);
		if (error)
			goto err_mutex;

		error = regmap_write(cs40l26->regmap, reg, value);
	}

	if (error)
		goto err_mutex;

	error = count;

err_mutex:
	cs40l26->comp_enable_pend = false;
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error;
}
static DEVICE_ATTR_RW(redc_comp_enable);

static ssize_t swap_firmware_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error;

	mutex_lock(&cs40l26->lock);

	if (cs40l26->fw_id == CS40L26_FW_ID)
		error = snprintf(buf, PAGE_SIZE, "%d\n", 0);
	else if (cs40l26->fw_id == CS40L26_FW_CALIB_ID)
		error = snprintf(buf, PAGE_SIZE, "%d\n", 1);
	else
		error = -EINVAL;

	mutex_unlock(&cs40l26->lock);

	return error;
}

static ssize_t swap_firmware_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error;
	unsigned int variant;

	error = kstrtou32(buf, 10, &variant);
	if (error)
		return error;

	if (variant == 0)
		error = cs40l26_fw_swap(cs40l26, CS40L26_FW_ID);
	else if (variant == 1)
		error = cs40l26_fw_swap(cs40l26, CS40L26_FW_CALIB_ID);
	else
		error = -EINVAL;

	return error ? error : count;
}
static DEVICE_ATTR_RW(swap_firmware);

static ssize_t swap_wavetable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error;

	mutex_lock(&cs40l26->lock);

	error = snprintf(buf, PAGE_SIZE, "%u\n", cs40l26->wt_num);

	mutex_unlock(&cs40l26->lock);

	return error;
}

static ssize_t swap_wavetable_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error;

	/* Bypass PM runtime framework for DSP shutdown & wake */
	cs40l26_irq_enable(cs40l26, CS40L26_IRQ_DISABLE);
	cs40l26_pm_runtime_teardown(cs40l26);

	mutex_lock(&cs40l26->lock);

	error = kstrtou32(buf, 10, &cs40l26->wt_num);
	if (error)
		goto err_setup;

	error = cs40l26_wt_swap(cs40l26);
	if (error)
		goto err_setup;

	mutex_unlock(&cs40l26->lock);

	error = cs40l26_pm_runtime_setup(cs40l26);
	if (error)
		return error;

	cs40l26_irq_enable(cs40l26, CS40L26_IRQ_ENABLE);

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cs40l26_mailbox_write(cs40l26, CS40L26_DSP_MBOX_CMD_OWT_RESET);
	if (error)
		goto err_mutex;

	dev_info(cs40l26->dev, "Loaded new wavetable with %d waveforms\n",
			cs40l26_num_waves(cs40l26));

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error ? error : count;

err_setup:
	mutex_unlock(&cs40l26->lock);

	if (cs40l26_pm_runtime_setup(cs40l26))
		dev_err(cs40l26->dev, "Failed to re-initialize PM runtime\n");

	cs40l26_irq_enable(cs40l26, CS40L26_IRQ_ENABLE);

	return error;
}
static DEVICE_ATTR_RW(swap_wavetable);

static ssize_t fw_rev_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	unsigned int val;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);
	error = cl_dsp_fw_rev_get(cs40l26->dsp, &val);
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;

	return snprintf(buf, PAGE_SIZE, "%d.%d.%d\n",
			(int) CL_DSP_GET_MAJOR(val),
			(int) CL_DSP_GET_MINOR(val),
			(int) CL_DSP_GET_PATCH(val));
}
static DEVICE_ATTR_RO(fw_rev);

static ssize_t init_rom_wavetable_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 enable;
	int error;

	error = kstrtou32(buf, 10, &enable);
	if (error)
		return error;

	if (enable != 1)
		return -EINVAL;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cs40l26_rom_wt_init(cs40l26);

	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error ? error : count;
}
static DEVICE_ATTR_WO(init_rom_wavetable);

static int cs40l26_braking_time_find(struct cs40l26_private *cs40l26, struct cl_dsp_memchunk *ch,
		u32 index, u32 current_index)
{
	u32 metadata_word = 0, period = 0;
	int error;

	/*
	 * In the OWT wavetable, the metadata is appended to the end
	 * of the waveform header section.
	 */
	do {
		error = cl_dsp_memchunk_read(cs40l26->dsp, ch, 24, &metadata_word);
		if (error)
			return error;

		if (FIELD_GET(CL_DSP_MD_TYPE_MASK, metadata_word) == CL_DSP_SVC_ID &&
				FIELD_GET(CL_DSP_MD_LENGTH_MASK, metadata_word) ==
				CL_DSP_SVC_LEN && (index == current_index)) {
			/* Braking period is second word of SVC metadata */
			error = cl_dsp_memchunk_read(cs40l26->dsp, ch, 24, &period);
			if (error)
				return error;

			/* Braking period is stored as milliseconds * 8 */
			return (period / 8);
		}
	} while (metadata_word != CL_DSP_MD_TERMINATOR);

	return 0;
}

static int cs40l26_owt_size_get(struct cs40l26_private *cs40l26)
{
	u32 offset, offset_reg, owt_base, owt_base_reg;
	int error;

	error = cl_dsp_get_reg(cs40l26->dsp, "OWT_BASE_XM", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_VIBEGEN_ALGO_ID, &owt_base_reg);
	if (error)
		return error;

	error = cl_dsp_get_reg(cs40l26->dsp, "OWT_NEXT_XM", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_VIBEGEN_ALGO_ID, &offset_reg);
	if (error)
		return error;

	error = regmap_read(cs40l26->regmap, offset_reg, &offset);
	if (error)
		return error;

	error = regmap_read(cs40l26->regmap, owt_base_reg, &owt_base);
	if (error)
		return error;

	if (owt_base > offset)
		return -ENOMEM;

	return ((offset - owt_base) * CL_DSP_BYTES_PER_WORD);
}

static int cs40l26_owt_braking_time_get(struct cs40l26_private *cs40l26, u32 index,
		unsigned int *braking_time)
{
	u32 current_index = 0, size = 0;
	u32 owt_base, owt_base_reg, wavetable_reg;
	struct cl_dsp_memchunk ch;
	int error, owt_size_bytes;
	u8 type, *wavetable;
	u16 flags;

	owt_size_bytes = cs40l26_owt_size_get(cs40l26);
	if (owt_size_bytes < 0)
		return owt_size_bytes;

	error = cl_dsp_get_reg(cs40l26->dsp, "OWT_BASE_XM", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_VIBEGEN_ALGO_ID, &owt_base_reg);
	if (error)
		return error;

	error = regmap_read(cs40l26->regmap, owt_base_reg, &owt_base);
	if (error)
		return error;

	wavetable = kmalloc(owt_size_bytes, GFP_KERNEL);
	if (!wavetable)
		return -ENOMEM;

	error = cl_dsp_get_reg(cs40l26->dsp, "WAVE_XM_TABLE", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_VIBEGEN_ALGO_ID, &wavetable_reg);
	if (error)
		goto wt_free;

	error = regmap_raw_read(cs40l26->regmap, wavetable_reg +
			(owt_base * CL_DSP_BYTES_PER_WORD), wavetable, owt_size_bytes);
	if (error)
		goto wt_free;

	ch = cl_dsp_memchunk_create(wavetable, owt_size_bytes);

	/* Ensure there's enough unread space to read at least one header */
	while ((ch.max - ch.data) >= CS40L26_WT_HEADER_PWLE_SIZE) {
		error = cl_dsp_memchunk_read(cs40l26->dsp, &ch, 16, &flags);
		if (error)
			goto wt_free;

		error = cl_dsp_memchunk_read(cs40l26->dsp, &ch, 8, &type);
		if (error)
			goto wt_free;

		if (type != WT_TYPE_V6_PWLE && current_index == index) {
			dev_err(cs40l26->dev, "Braking time only available for PWLE effects\n");
			error = -EINVAL;
			cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_SYSFS, __func__);
			goto wt_free;
		}

		error = cl_dsp_memchunk_read(cs40l26->dsp, &ch, 24, NULL);
		if (error)
			goto wt_free;

		error = cl_dsp_memchunk_read(cs40l26->dsp, &ch, 24, &size);
		if (error)
			goto wt_free;

		if (flags & CL_DSP_MD_PRESENT) {
			error = cs40l26_braking_time_find(cs40l26, &ch,
					index, current_index);
			if (error < 0) {
				goto wt_free;
			} else if (error > 0) {
				*braking_time = error;
				error = 0;
				goto wt_free;
			}
		} else if (type == WT_TYPE_V6_PWLE) {
			/* Skip header terminator word */
			error = cl_dsp_memchunk_read(cs40l26->dsp, &ch, 24, NULL);
			if (error)
				goto wt_free;
		}

		/* Skip to the start of the next waveform's header */
		while (size-- > 0) {
			error = cl_dsp_memchunk_read(cs40l26->dsp, &ch, 24, NULL);
			if (error)
				goto wt_free;
		}

		/* Exit if we are looking past the desired index */
		if (++current_index > index) {
			error = -EINVAL;
			goto wt_free;
		}
	}
wt_free:
	if (error) {
		dev_err(cs40l26->dev, "Braking time not found for OWT index %u\n", index);
		cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_DSP, __func__);
	}

	kfree(wavetable);

	return error;
}

static ssize_t braking_time_bank_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error;
	u32 bank;

	error = kstrtou32(buf, 10, &bank);
	if (error)
		return error;

	if (bank != CS40L26_RAM_BANK_ID && bank != CS40L26_OWT_BANK_ID) {
		dev_err(cs40l26->dev, "Bank %u unsupported\n", bank);
		return cs40l26_log_err(cs40l26, -EINVAL, CS40L26_ERR_TYPE_SYSFS, __func__);
	}

	mutex_lock(&cs40l26->lock);

	cs40l26->braking_time_bank = bank;

	mutex_unlock(&cs40l26->lock);

	return count;
}
static DEVICE_ATTR_WO(braking_time_bank);

static ssize_t braking_time_index_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error;
	u32 index;

	error = kstrtou32(buf, 10, &index);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	cs40l26->braking_time_index = index;

	mutex_unlock(&cs40l26->lock);

	return count;
}
static DEVICE_ATTR_WO(braking_time_index);

static ssize_t braking_time_ms_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 index = cs40l26->braking_time_index;
	unsigned int braking_time = 0;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	switch (cs40l26->braking_time_bank) {
	case CS40L26_RAM_BANK_ID:
		if (index > (cs40l26_num_ram_waves(cs40l26) - 1)) {
			dev_err(cs40l26->dev, "Index exceeds number of RAM effects\n");
			error = -EINVAL;
			cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_SYSFS, __func__);
			goto err_mutex;
		}

		braking_time = cs40l26->dsp->wt_desc->owt.waves[index].braking_time;
		break;
	case CS40L26_OWT_BANK_ID:
		if (index > (cs40l26_num_owt_waves(cs40l26) - 1)) {
			dev_err(cs40l26->dev, "Index exceeds number of OWT effects\n");
			error = -EINVAL;
			cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_SYSFS, __func__);
			goto err_mutex;
		}

		error = cs40l26_owt_braking_time_get(cs40l26, index, &braking_time);
		break;
	default:
		dev_err(cs40l26->dev, "Bank %u unsupported\n", cs40l26->braking_time_bank);
		error = -EINVAL;
		cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_SYSFS, __func__);
		break;
	}

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error ? error : snprintf(buf, PAGE_SIZE, "%u\n", braking_time);
}
static DEVICE_ATTR_RO(braking_time_ms);

static ssize_t error_log_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int count, error = 0, tmp_count;
	char tmp_str[CS40L26_ERR_STR_MAX_LEN];
	struct cs40l26_err *err;
	ssize_t str_size;
	size_t nelements;
	char *str;

	mutex_lock(&cs40l26->lock);

	nelements = list_count_nodes(&cs40l26->err_head);
	if (!nelements) {
		dev_err(cs40l26->dev, "Error Log is Empty\n");
		error = -ENODATA;
		goto mutex_exit;
	}

	str_size = nelements * CS40L26_ERR_STR_MAX_LEN;

	str = kzalloc(str_size, GFP_KERNEL);
	if (!str) {
		error = -ENOMEM;
		goto mutex_exit;
	}

	list_for_each_entry(err, &cs40l26->err_head, list) {
		tmp_count = snprintf(tmp_str, CS40L26_ERR_STR_MAX_LEN,
				"%s: code = %d, type = %u\n", err->fxn_name, err->code, err->type);
		if (tmp_count != strlen(tmp_str)) {
			dev_err(cs40l26->dev, "Failed to format error string\n");
			if (tmp_count < 0)
				error = tmp_count;
			else
				error = -EINVAL;

			goto free_exit;
		}

		strncat(str, tmp_str, CS40L26_ERR_STR_MAX_LEN);
	}

	count = snprintf(buf, PAGE_SIZE, str);

free_exit:
	kfree(str);

mutex_exit:
	mutex_unlock(&cs40l26->lock);

	return error ? error : count;
}

static ssize_t error_log_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	struct cs40l26_err *err, *err_tmp;
	int error;
	u32 val;

	error = kstrtou32(buf, 10, &val);
	if (error)
		return error;

	if (val <= 0)
		return -EINVAL;

	mutex_lock(&cs40l26->lock);

	list_for_each_entry_safe(err, err_tmp, &cs40l26->err_head, list)
		list_del(&err->list);

	mutex_unlock(&cs40l26->lock);

	return count;
}
static DEVICE_ATTR_RW(error_log);

static struct attribute *cs40l26_dev_attrs[] = {
	&dev_attr_broadcast_master.attr,
	&dev_attr_num_waves.attr,
	&dev_attr_die_temp.attr,
	&dev_attr_owt_free_space.attr,
	&dev_attr_dsp_state.attr,
	&dev_attr_halo_heartbeat.attr,
	&dev_attr_pm_stdby_timeout_ms.attr,
	&dev_attr_pm_active_timeout_ms.attr,
	&dev_attr_vibe_state.attr,
	&dev_attr_f0_offset.attr,
	&dev_attr_delay_before_stop_playback_us.attr,
	&dev_attr_f0_comp_enable.attr,
	&dev_attr_redc_comp_enable.attr,
	&dev_attr_swap_firmware.attr,
	&dev_attr_swap_wavetable.attr,
	&dev_attr_fw_rev.attr,
	&dev_attr_owt_lib_compat.attr,
	&dev_attr_overprotection_gain.attr,
	&dev_attr_init_rom_wavetable.attr,
	&dev_attr_braking_time_bank.attr,
	&dev_attr_braking_time_index.attr,
	&dev_attr_braking_time_ms.attr,
	&dev_attr_error_log.attr,
	NULL,
};

static struct attribute_group cs40l26_dev_attr_group = {
	.name = "default",
	.attrs = cs40l26_dev_attrs,
};

static ssize_t trigger_calibration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 mailbox_command, calibration_request_payload;
	int error;
	struct completion *completion;

	dev_dbg(cs40l26->dev, "%s: %s", __func__, buf);

	if (!cs40l26->calib_fw) {
		dev_err(cs40l26->dev, "Must use calibration firmware\n");
		return cs40l26_log_err(cs40l26, -EPERM, CS40L26_ERR_TYPE_FW, __func__);
	}

	error = kstrtou32(buf, 16, &calibration_request_payload);
	if (error)
		return -EINVAL;

	switch (calibration_request_payload) {
	case CS40L26_CALIBRATION_CONTROL_REQUEST_F0_AND_Q:
		completion = &cs40l26->cal_f0_cont;
		break;
	case CS40L26_CALIBRATION_CONTROL_REQUEST_REDC:
		completion = &cs40l26->cal_redc_cont;
		break;
	case CS40L26_CALIBRATION_CONTROL_REQUEST_DVL_PEQ:
		completion = &cs40l26->cal_dvl_peq_cont;
		break;
	case CS40L26_CALIBRATION_CONTROL_REQUEST_LS_CALIBRATION:
		completion = &cs40l26->cal_ls_cont;
		break;
	default:
		return -EINVAL;
	}

	mailbox_command = ((CS40L26_DSP_MBOX_CMD_INDEX_CALIBRATION_CONTROL <<
			CS40L26_DSP_MBOX_CMD_TYPE_SHIFT) & CS40L26_DSP_MBOX_CMD_TYPE_MASK) |
			(calibration_request_payload & CS40L26_DSP_MBOX_CMD_PAYLOAD_MASK);

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);
	reinit_completion(completion);

	error = cs40l26_mailbox_write(cs40l26, mailbox_command);

	mutex_unlock(&cs40l26->lock);

	if (error) {
		dev_err(cs40l26->dev, "Failed to request calibration\n");
		cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_DSP, __func__);
		goto err_pm;
	}

	if (!wait_for_completion_timeout(
			completion,
			msecs_to_jiffies(CS40L26_CALIBRATION_TIMEOUT_MS))) {
		error = -ETIME;
		dev_err(cs40l26->dev, "Failed to complete cal req, %d, err: %d",
				calibration_request_payload, error);
		cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_DSP, __func__);
		goto err_pm;
	}

	mutex_lock(&cs40l26->lock);

	if (calibration_request_payload == CS40L26_CALIBRATION_CONTROL_REQUEST_F0_AND_Q)
		error = cs40l26_copy_f0_est_to_dvl(cs40l26);

	mutex_unlock(&cs40l26->lock);
err_pm:
	cs40l26_pm_exit(cs40l26->dev);
	return error ? error : count;
}
static DEVICE_ATTR_WO(trigger_calibration);

static ssize_t f0_measured_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 reg, f0_measured;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "F0_EST", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_F0_EST_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_read(cs40l26->regmap, reg, &f0_measured);
	if (error)
		goto err_mutex;

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;
	else
		return snprintf(buf, PAGE_SIZE, "%08X\n", f0_measured);
}
static DEVICE_ATTR_RO(f0_measured);

static ssize_t q_measured_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 reg, q_measured;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "Q_EST", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_F0_EST_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_read(cs40l26->regmap, reg, &q_measured);
	if (error)
		goto err_mutex;

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;
	else
		return snprintf(buf, PAGE_SIZE, "%08X\n", q_measured);
}
static DEVICE_ATTR_RO(q_measured);

static ssize_t redc_measured_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 reg, redc_measured;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "RE_EST_STATUS", CL_DSP_YM_UNPACKED_TYPE,
			CS40L26_SVC_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_read(cs40l26->regmap, reg, &redc_measured);
	if (error)
		goto err_mutex;

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;
	else
		return snprintf(buf, PAGE_SIZE, "%08X\n", redc_measured);
}
static DEVICE_ATTR_RO(redc_measured);

static ssize_t redc_est_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 reg, redc_est;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "REDC", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_F0_EST_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_read(cs40l26->regmap, reg, &redc_est);
	if (error)
		goto err_mutex;

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;
	else
		return snprintf(buf, PAGE_SIZE, "%08X\n", redc_est);
}

static ssize_t redc_est_store(struct device *dev, struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 reg, redc_est;
	int error;

	dev_dbg(cs40l26->dev, "%s: %s", __func__, buf);

	error = kstrtou32(buf, 16, &redc_est);
	if (error)
		return error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "REDC", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_F0_EST_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_write(cs40l26->regmap, reg, redc_est);
	if (error)
		goto err_mutex;

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;
	else
		return count;
}
static DEVICE_ATTR_RW(redc_est);

static ssize_t f0_stored_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 reg, f0_stored;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "F0_OTP_STORED", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_VIBEGEN_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_read(cs40l26->regmap, reg, &f0_stored);
	if (error)
		goto err_mutex;

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;
	else
		return snprintf(buf, PAGE_SIZE, "%08X\n", f0_stored);
}

static ssize_t f0_stored_store(struct device *dev, struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 reg, f0_stored;
	int error;

	dev_dbg(cs40l26->dev, "%s: %s", __func__, buf);

	error = kstrtou32(buf, 16, &f0_stored);
	if (error)
		return error;

	if (f0_stored < CS40L26_F0_FREQ_CENTRE_MIN || f0_stored > CS40L26_F0_FREQ_CENTRE_MAX)
		return -EINVAL;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "F0_OTP_STORED", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_VIBEGEN_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_write(cs40l26->regmap, reg, f0_stored);
	if (error)
		goto err_mutex;

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;
	else
		return count;
}
static DEVICE_ATTR_RW(f0_stored);

static ssize_t redc_stored_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 reg, redc_stored;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "REDC_OTP_STORED", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_VIBEGEN_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_read(cs40l26->regmap, reg, &redc_stored);
	if (error)
		goto err_mutex;

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;
	else
		return snprintf(buf, PAGE_SIZE, "%08X\n", redc_stored);
}

static ssize_t redc_stored_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 reg, redc_stored;
	int error;

	dev_dbg(cs40l26->dev, "%s: %s", __func__, buf);

	error = kstrtou32(buf, 16, &redc_stored);
	if (error)
		return error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "REDC_OTP_STORED", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_VIBEGEN_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_write(cs40l26->regmap, reg, redc_stored);
	if (error)
		goto err_mutex;

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;
	else
		return count;
}
static DEVICE_ATTR_RW(redc_stored);

static ssize_t freq_centre_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 freq_centre, reg;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "FREQ_CENTRE", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_F0_EST_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_read(cs40l26->regmap, reg, &freq_centre);

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error ? error : snprintf(buf, PAGE_SIZE, "%08X\n", freq_centre);
}

static ssize_t freq_centre_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 freq_centre, reg;
	int error;

	error = kstrtou32(buf, 16, &freq_centre);
	if (error)
		return error;

	if (freq_centre < CS40L26_F0_FREQ_CENTRE_MIN ||
			freq_centre > CS40L26_F0_FREQ_CENTRE_MAX)
		return -EINVAL;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "FREQ_CENTRE", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_F0_EST_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_write(cs40l26->regmap, reg, freq_centre);

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error ? error : count;
}
static DEVICE_ATTR_RW(freq_centre);

static ssize_t freq_span_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error, freq_span;
	u32 reg;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "FREQ_SPAN", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_F0_EST_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_read(cs40l26->regmap, reg, &freq_span);

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error ? error : snprintf(buf, PAGE_SIZE, "%08X\n", freq_span);
}

static ssize_t freq_span_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error, s_freq_span;
	u32 freq_span, reg;

	error = kstrtou32(buf, 16, &freq_span);
	if (error)
		return error;

	freq_span &= GENMASK(23, 0);
	s_freq_span = (freq_span & BIT(23)) ? (freq_span | GENMASK(31, 24)) : freq_span;

	if (abs(s_freq_span) < CS40L26_F0_FREQ_SPAN_MIN ||
			abs(s_freq_span) > CS40L26_F0_FREQ_SPAN_MAX)
		return -EINVAL;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "FREQ_SPAN", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_F0_EST_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_write(cs40l26->regmap, reg, freq_span);

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error ? error : count;
}
static DEVICE_ATTR_RW(freq_span);

static ssize_t f0_and_q_cal_time_ms_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 reg, tone_dur_ms, freq_centre, freq_span;
	int error, f0_and_q_cal_time_ms;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "TONE_DURATION_MS", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_F0_EST_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_read(cs40l26->regmap, reg, &tone_dur_ms);
	if (error) {
		dev_err(cs40l26->dev, "Failed to get tone duration\n");
		cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_CP, __func__);
		goto err_mutex;
	}

	if (tone_dur_ms == 0) { /* Calculate value */
		error = cl_dsp_get_reg(cs40l26->dsp, "FREQ_SPAN", CL_DSP_XM_UNPACKED_TYPE,
				CS40L26_F0_EST_ALGO_ID, &reg);
		if (error) {
			cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_FW, __func__);
			goto err_mutex;
		}

		error = regmap_read(cs40l26->regmap, reg, &freq_span);
		if (error) {
			dev_err(cs40l26->dev, "Failed to get FREQ_SPAN\n");
			cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_CP, __func__);
			goto err_mutex;
		}

		error = cl_dsp_get_reg(cs40l26->dsp, "FREQ_CENTRE", CL_DSP_XM_UNPACKED_TYPE,
				CS40L26_F0_EST_ALGO_ID, &reg);
		if (error)
			goto err_mutex;

		error = regmap_read(cs40l26->regmap, reg, &freq_centre);
		if (error) {
			dev_err(cs40l26->dev, "Failed to get FREQ_CENTRE\n");
			cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_CP, __func__);
			goto err_mutex;
		}

		f0_and_q_cal_time_ms = ((CS40L26_F0_CHIRP_DURATION_FACTOR *
				(int) (freq_span >> CS40L26_F0_EST_FREQ_FRAC_BITS)) /
				(int) (freq_centre >> CS40L26_F0_EST_FREQ_FRAC_BITS));
	} else if (tone_dur_ms < CS40L26_F0_AND_Q_CALIBRATION_MIN_MS) {
		f0_and_q_cal_time_ms = CS40L26_F0_AND_Q_CALIBRATION_MIN_MS;
	} else if (tone_dur_ms > CS40L26_F0_AND_Q_CALIBRATION_MAX_MS) {
		f0_and_q_cal_time_ms = CS40L26_F0_AND_Q_CALIBRATION_MAX_MS;
	} else {
		f0_and_q_cal_time_ms = tone_dur_ms;
	}

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;
	else
		return snprintf(buf, PAGE_SIZE, "%d\n", f0_and_q_cal_time_ms);
}
static DEVICE_ATTR_RO(f0_and_q_cal_time_ms);

static ssize_t redc_cal_time_ms_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* FIRMWARE_STUMPY_CALIB_REDC_PLAYTIME_MS + SVC_INIT + buffer */
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 reg, redc_playtime_ms, redc_total_cal_time_ms;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "REDC_PLAYTIME_MS", CL_DSP_XM_UNPACKED_TYPE,
			cs40l26->fw_id, &reg);
	if (error)
		goto err_mutex;

	error = regmap_read(cs40l26->regmap, reg, &redc_playtime_ms);

	redc_total_cal_time_ms = redc_playtime_ms + CS40L26_SVC_INITIALIZATION_PERIOD_MS +
			CS40L26_REDC_CALIBRATION_BUFFER_MS;

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;
	else
		return snprintf(buf, PAGE_SIZE, "%d\n", redc_total_cal_time_ms);
}
static DEVICE_ATTR_RO(redc_cal_time_ms);

static ssize_t dvl_peq_coefficients_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	u32 reg, dvl_peq_coefficients[CS40L26_DVL_PEQ_COEFFICIENTS_NUM_REGS];
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "PEQ_COEF1_X", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_DVL_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_bulk_read(cs40l26->regmap, reg, dvl_peq_coefficients,
			CS40L26_DVL_PEQ_COEFFICIENTS_NUM_REGS);
	if (error)
		goto err_mutex;

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;

	return snprintf(buf, PAGE_SIZE, "%08X %08X %08X %08X %08X %08X\n",
			dvl_peq_coefficients[0], dvl_peq_coefficients[1], dvl_peq_coefficients[2],
			dvl_peq_coefficients[3], dvl_peq_coefficients[4], dvl_peq_coefficients[5]);
}

static ssize_t dvl_peq_coefficients_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 reg, dvl_peq_coefficients[CS40L26_DVL_PEQ_COEFFICIENTS_NUM_REGS];
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	char *coeffs_str, *coeffs_str_temp, *coeff_str;
	int error, coeffs_found = 0;

	coeffs_str = kstrdup(buf, GFP_KERNEL);
	if (!coeffs_str)
		return -ENOMEM;

	coeffs_str_temp = coeffs_str;
	while ((coeff_str = strsep(&coeffs_str_temp, " ")) != NULL) {
		error = kstrtou32(coeff_str, 16, &dvl_peq_coefficients[coeffs_found++]);
		if (error)
			goto err_free;
	}

	if (coeffs_found != CS40L26_DVL_PEQ_COEFFICIENTS_NUM_REGS) {
		dev_err(cs40l26->dev, "Num DVL PEQ coeffs, %d, expecting %d\n",
				coeffs_found, CS40L26_DVL_PEQ_COEFFICIENTS_NUM_REGS);
		error = -EINVAL;
		cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_COEFF, __func__);
		goto err_free;
	}

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		goto err_free;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "PEQ_COEF1_X", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_DVL_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_bulk_write(cs40l26->regmap, reg, dvl_peq_coefficients,
			CS40L26_DVL_PEQ_COEFFICIENTS_NUM_REGS);
	if (error) {
		dev_err(cs40l26->dev, "Failed to write DVL PEQ coefficients,%d", error);
		cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_CP, __func__);
	}

err_mutex:
	mutex_unlock(&cs40l26->lock);
	cs40l26_pm_exit(cs40l26->dev);
err_free:
	kfree(coeffs_str);
	return error ? error : count;
}
static DEVICE_ATTR_RW(dvl_peq_coefficients);

static ssize_t ls_calibration_params_temp_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 params_temp, reg;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "PARAMS_TEMPERATURE", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_LS_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_read(cs40l26->regmap, reg, &params_temp);

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error ? error : snprintf(buf, PAGE_SIZE, "0x%06X\n", params_temp);
}

static ssize_t ls_calibration_params_temp_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 params_temp, reg;
	int error;

	error = kstrtou32(buf, 16, &params_temp);
	if (error)
		return error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "PARAMS_TEMPERATURE", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_LS_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_write(cs40l26->regmap, reg, params_temp);

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error ? error : count;
}
static DEVICE_ATTR_RW(ls_calibration_params_temp);

static int cs40l26_ls_calibration_check_results(struct cs40l26_private *cs40l26)
{
	u32 reg, return_code;
	int error;

	error = cl_dsp_get_reg(cs40l26->dsp, "STATE_CAL_RETURN_CODE", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_LS_ALGO_ID, &reg);
	if (error)
		return error;

	error = regmap_read(cs40l26->regmap, reg, &return_code);
	if (error)
		return error;

	switch (return_code) {
	case CS40L26_LS_CAL_OK:
		dev_dbg(cs40l26->dev, "LS Calibration Succeeded\n");
		break;
	case CS40L26_LS_CAL_IN_PROGRESS:
		dev_err(cs40l26->dev, "LS Calibration still in progress\n");
		return cs40l26_log_err(cs40l26, -EALREADY, CS40L26_ERR_TYPE_DSP, __func__);
	case CS40L26_LS_CAL_FAIL_DET:
		dev_err(cs40l26->dev, "LS Calibration failed: matrix singular/nearly singular\n");
		return cs40l26_log_err(cs40l26, -EINVAL, CS40L26_ERR_TYPE_DSP, __func__);
	case CS40L26_LS_CAL_FAIL_ROOTS:
		dev_err(cs40l26->dev, "LS Calibration failed: real roots instead of 1\n");
		return cs40l26_log_err(cs40l26, -EINVAL, CS40L26_ERR_TYPE_DSP, __func__);
	case CS40L26_LS_CAL_SATURATION:
		dev_err(cs40l26->dev, "LS Calibration failed: saturation when publishing\n");
		return cs40l26_log_err(cs40l26, -EINVAL, CS40L26_ERR_TYPE_DSP, __func__);
	case CS40L26_LS_CAL_STEP2_FREQ:
		dev_err(cs40l26->dev, "LS Calibration failed: frequency for step 2 out of range\n");
		return cs40l26_log_err(cs40l26, -EINVAL, CS40L26_ERR_TYPE_DSP, __func__);
	default:
		dev_err(cs40l26->dev, "LS Calibration failed: unknown error code %u\n",
				return_code);
		return cs40l26_log_err(cs40l26, -EINVAL, CS40L26_ERR_TYPE_DSP, __func__);
	}

	return 0;
}

static ssize_t ls_calibration_results_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int count = 0, error, i;
	u32 reg, val;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cs40l26_ls_calibration_check_results(cs40l26);
	if (error)
		goto err_mutex;

	for (i = 0; i < CS40L26_LS_CAL_NUM_REGS; i++) {
		error = cl_dsp_get_reg(cs40l26->dsp, cs40l26_ls_cal_params[i].calib_name,
				CL_DSP_XM_UNPACKED_TYPE, CS40L26_LS_ALGO_ID, &reg);
		if (error)
			goto err_mutex;

		if (cs40l26_ls_cal_params[i].word_num == 2)
			reg += sizeof(u32);

		error = regmap_read(cs40l26->regmap, reg, &val);
		if (error)
			goto err_mutex;

		count += snprintf(&buf[count], PAGE_SIZE, "0x%06X\n", val);
	}

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error ? error : count;
}

static ssize_t ls_calibration_results_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error, i, results_found = 0;
	u32 reg, results[CS40L26_LS_CAL_NUM_REGS];
	char *str, *str_full;

	str_full = kstrdup(buf, GFP_KERNEL);
	if (!str_full)
		return -ENOMEM;

	while ((str = strsep(&str_full, "\n")) != NULL &&
			results_found < CS40L26_LS_CAL_NUM_REGS) {
		error = kstrtou32(str, 16, &results[results_found++]);
		if (error)
			goto err_free;
	}

	if (results_found != CS40L26_LS_CAL_NUM_REGS) {
		dev_err(cs40l26->dev, "Num results = %d, expecting %d\n",
				results_found, CS40L26_LS_CAL_NUM_REGS);
		error = -EINVAL;
		goto err_free;
	}

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		goto err_free;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "CFG", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_EP_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_set_bits(cs40l26->regmap, reg, CS40L26_LS_CAL_REINIT_MASK);
	if (error)
		goto err_mutex;

	for (i = 0; i < results_found; i++) {
		if (cs40l26_ls_cal_params[i].runtime_name == NULL)
			continue;

		error = cl_dsp_get_reg(cs40l26->dsp, cs40l26_ls_cal_params[i].runtime_name,
				CL_DSP_XM_UNPACKED_TYPE, CS40L26_EP_ALGO_ID, &reg);
		if (error)
			goto err_mutex;

		if (cs40l26_ls_cal_params[i].word_num == 2)
			reg += sizeof(u32);

		error = regmap_write(cs40l26->regmap, reg, results[i]);
		if (error)
			goto err_mutex;
	}

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);
err_free:
	kfree(str_full);

	return error ? error : count;
}
static DEVICE_ATTR_RW(ls_calibration_results);

static ssize_t ls_calibration_results_name_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int count = 0, error, i;
	u32 reg, val;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cs40l26_ls_calibration_check_results(cs40l26);
	if (error)
		goto err_mutex;

	for (i = 0; i < CS40L26_LS_CAL_NUM_REGS; i++) {
		error = cl_dsp_get_reg(cs40l26->dsp, cs40l26_ls_cal_params[i].calib_name,
				CL_DSP_XM_UNPACKED_TYPE, CS40L26_LS_ALGO_ID, &reg);
		if (error)
			goto err_mutex;

		if (cs40l26_ls_cal_params[i].word_num == 2)
			reg += sizeof(u32);

		error = regmap_read(cs40l26->regmap, reg, &val);
		if (error)
			goto err_mutex;

		count += snprintf(&buf[count], PAGE_SIZE, "%s: 0x%06X\n",
				cs40l26_ls_cal_params[i].calib_name, val);
	}

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error ? error : count;
}
static DEVICE_ATTR_RO(ls_calibration_results_name);

static ssize_t svc_le_est_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	unsigned int le;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cs40l26_svc_le_estimate(cs40l26, &le);

	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	if (error)
		return error;

	return snprintf(buf, PAGE_SIZE, "%u\n", le);
}
static DEVICE_ATTR_RO(svc_le_est);

static ssize_t svc_le_stored_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error;

	mutex_lock(&cs40l26->lock);

	error = snprintf(buf, PAGE_SIZE, "%d\n", cs40l26->svc_le_est_stored);

	mutex_unlock(&cs40l26->lock);

	return error;
}

static ssize_t svc_le_stored_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 svc_le_stored;
	int error;

	error = kstrtou32(buf, 10, &svc_le_stored);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	cs40l26->svc_le_est_stored = svc_le_stored;

	mutex_unlock(&cs40l26->lock);

	return count;
}
static DEVICE_ATTR_RW(svc_le_stored);

static struct attribute *cs40l26_dev_attrs_cal[] = {
	&dev_attr_svc_le_est.attr,
	&dev_attr_svc_le_stored.attr,
	&dev_attr_trigger_calibration.attr,
	&dev_attr_f0_measured.attr,
	&dev_attr_q_measured.attr,
	&dev_attr_redc_measured.attr,
	&dev_attr_ls_calibration_params_temp.attr,
	&dev_attr_ls_calibration_results.attr,
	&dev_attr_ls_calibration_results_name.attr,
	&dev_attr_dvl_peq_coefficients.attr,
	&dev_attr_redc_est.attr,
	&dev_attr_f0_stored.attr,
	&dev_attr_redc_stored.attr,
	&dev_attr_freq_centre.attr,
	&dev_attr_freq_span.attr,
	&dev_attr_f0_and_q_cal_time_ms.attr,
	&dev_attr_redc_cal_time_ms.attr,
	NULL,
};

static struct attribute_group cs40l26_dev_attr_cal_group = {
	.name = "calibration",
	.attrs = cs40l26_dev_attrs_cal,
};

static ssize_t logging_en_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 reg, enable;
	int error;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "ENABLE", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_LOGGER_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_read(cs40l26->regmap, reg, &enable);

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error ? error : snprintf(buf, PAGE_SIZE, "%u\n", enable);
}

static ssize_t logging_en_store(struct device *dev, struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 enable, reg;
	int error;

	error = kstrtou32(buf, 10, &enable);
	if (error)
		return error;

	enable &= CS40L26_LOGGER_EN_MASK;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cl_dsp_get_reg(cs40l26->dsp, "ENABLE", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_LOGGER_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	error = regmap_write(cs40l26->regmap, reg, enable);

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error ? error : count;
}
static DEVICE_ATTR_RW(logging_en);

static ssize_t logging_max_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 rst;
	int error;

	error = kstrtou32(buf, 10, &rst);
	if (error)
		return error;

	if (rst != 1)
		return -EINVAL;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	cs40l26_mailbox_write(cs40l26, CS40L26_DSP_MBOX_CMD_LOGGER_MAX_RESET);

	cs40l26_pm_exit(cs40l26->dev);

	return count;
}
static DEVICE_ATTR_WO(logging_max_reset);

static ssize_t available_logger_srcs_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	char log_srcs[25] = "";
	int i;

	for (i = 0; i < cs40l26->num_log_srcs; i++) {
		switch (cs40l26->log_srcs[i].id) {
		case CS40L26_LOGGER_SRC_ID_BEMF:
			strncat(log_srcs, "BEMF\n", 5);
			break;
		case CS40L26_LOGGER_SRC_ID_VBST:
			strncat(log_srcs, "VBST\n", 5);
			break;
		case CS40L26_LOGGER_SRC_ID_VMON:
			strncat(log_srcs, "VMON\n", 5);
			break;
		case CS40L26_LOGGER_SRC_ID_EP:
			strncat(log_srcs, "EP\n", 3);
			break;
		case CS40L26_LOGGER_SRC_ID_IMON:
			strncat(log_srcs, "IMON\n", 5);
			break;
		default:
			dev_err(cs40l26->dev, "Invalid source ID %d\n", cs40l26->log_srcs[i].id);
			return cs40l26_log_err(cs40l26, -EINVAL, CS40L26_ERR_TYPE_FW, __func__);
		}
	}

	return snprintf(buf, PAGE_SIZE, "%s", log_srcs);
}
static DEVICE_ATTR_RO(available_logger_srcs);

static int cs40l26_logger_max_get(struct cs40l26_private *cs40l26, u32 src_id, u32 *max)
{
	int error, reg, src_num;
	u32 offset;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	for (src_num = 0; src_num < cs40l26->num_log_srcs; src_num++) {
		if (cs40l26->log_srcs[src_num].id == src_id)
			break;
	}

	if (src_num == cs40l26->num_log_srcs) {
		error = -ENODATA;
		goto err_mutex;
	}

	error = cl_dsp_get_reg(cs40l26->dsp, "DATA", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_LOGGER_ALGO_ID, &reg);
	if (error)
		goto err_mutex;

	offset = (src_num * CS40L26_LOGGER_DATA_MAX_STEP) + CS40L26_LOGGER_DATA_MAX_OFFSET;

	error = regmap_read(cs40l26->regmap, reg + offset, max);

err_mutex:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error;
}

static ssize_t max_bemf_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 max_bemf;
	int error;

	error = cs40l26_logger_max_get(cs40l26, CS40L26_LOGGER_SRC_ID_BEMF, &max_bemf);

	return error ? error : snprintf(buf, PAGE_SIZE, "0x%06X\n", max_bemf);
}
static DEVICE_ATTR_RO(max_bemf);

static ssize_t max_vbst_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 max_vbst;
	int error;

	error = cs40l26_logger_max_get(cs40l26, CS40L26_LOGGER_SRC_ID_VBST, &max_vbst);

	return error ? error : snprintf(buf, PAGE_SIZE, "0x%06X\n", max_vbst);
}
static DEVICE_ATTR_RO(max_vbst);

static ssize_t max_vmon_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 max_vmon;
	int error;

	error = cs40l26_logger_max_get(cs40l26, CS40L26_LOGGER_SRC_ID_VMON, &max_vmon);

	return error ? error : snprintf(buf, PAGE_SIZE, "0x%06X\n", max_vmon);
}
static DEVICE_ATTR_RO(max_vmon);

static ssize_t max_excursion_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 max_excursion;
	int error;

	error = cs40l26_logger_max_get(cs40l26, CS40L26_LOGGER_SRC_ID_EP, &max_excursion);

	return error ? error : snprintf(buf, PAGE_SIZE, "0x%06X\n", max_excursion);
}
static DEVICE_ATTR_RO(max_excursion);

static ssize_t max_imon_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 max_imon;
	int error;

	error = cs40l26_logger_max_get(cs40l26, CS40L26_LOGGER_SRC_ID_IMON, &max_imon);

	return error ? error : snprintf(buf, PAGE_SIZE, "0x%06X\n", max_imon);
}
DEVICE_ATTR_RO(max_imon);

static struct attribute *cs40l26_dev_attrs_dlog[] = {
	&dev_attr_logging_en.attr,
	&dev_attr_logging_max_reset.attr,
	&dev_attr_available_logger_srcs.attr,
	&dev_attr_max_bemf.attr,
	&dev_attr_max_vbst.attr,
	&dev_attr_max_vmon.attr,
	&dev_attr_max_excursion.attr,
	&dev_attr_max_imon.attr,
	NULL,
};

static struct attribute_group cs40l26_dev_attr_dlog_group = {
	.name = "data_logger",
	.attrs = cs40l26_dev_attrs_dlog,
};

static ssize_t fw_algo_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error;

	mutex_lock(&cs40l26->lock);

	error = snprintf(buf, PAGE_SIZE, "0x%06X\n", cs40l26->sysfs_fw.algo_id);

	mutex_unlock(&cs40l26->lock);

	return error;
}

static ssize_t fw_algo_id_store(struct device *dev, struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 algo_id;
	int error;

	error = kstrtou32(buf, 16, &algo_id);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	cs40l26->sysfs_fw.algo_id = algo_id;

	mutex_unlock(&cs40l26->lock);

	return count;
}
static DEVICE_ATTR_RW(fw_algo_id);

static ssize_t fw_ctrl_name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error;

	mutex_lock(&cs40l26->lock);

	error = snprintf(buf, PAGE_SIZE, "%s\n", cs40l26->sysfs_fw.ctrl_name);

	mutex_unlock(&cs40l26->lock);

	return error;
}

static ssize_t fw_ctrl_name_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);

	if (strlen(buf) > CS40L26_COEFF_NAME_MAX_LEN) {
		dev_err(cs40l26->dev, "Control name %s longer than 64 char limit\n", buf);
		return cs40l26_log_err(cs40l26, -E2BIG, CS40L26_ERR_TYPE_SYSFS, __func__);
	}

	mutex_lock(&cs40l26->lock);

	memset(cs40l26->sysfs_fw.ctrl_name, 0, CS40L26_COEFF_NAME_MAX_LEN);

	strscpy(cs40l26->sysfs_fw.ctrl_name, buf, count);

	mutex_unlock(&cs40l26->lock);

	return count;
}
static DEVICE_ATTR_RW(fw_ctrl_name);

static inline int cs40l26_sysfs_fw_get_reg(struct cs40l26_private *cs40l26, u32 *reg)
{
	return cl_dsp_get_reg(cs40l26->dsp, cs40l26->sysfs_fw.ctrl_name,
			cs40l26->sysfs_fw.block_type, cs40l26->sysfs_fw.algo_id, reg);
}

static inline int cs40l26_sysfs_fw_get_flags(struct cs40l26_private *cs40l26, unsigned int *flags)
{
	return cl_dsp_get_flags(cs40l26->dsp, cs40l26->sysfs_fw.ctrl_name,
			cs40l26->sysfs_fw.block_type, cs40l26->sysfs_fw.algo_id, flags);
}

static inline int cs40l26_sysfs_fw_get_length(struct cs40l26_private *cs40l26, size_t *nbytes)
{
	return cl_dsp_get_length(cs40l26->dsp, cs40l26->sysfs_fw.ctrl_name,
			cs40l26->sysfs_fw.block_type, cs40l26->sysfs_fw.algo_id, nbytes);
}

static ssize_t fw_ctrl_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error;
	u32 reg;

	mutex_lock(&cs40l26->lock);

	error = cs40l26_sysfs_fw_get_reg(cs40l26, &reg);

	mutex_unlock(&cs40l26->lock);

	return error ? error : snprintf(buf, PAGE_SIZE, "0x%08X\n", reg);
}
static DEVICE_ATTR_RO(fw_ctrl_reg);

static ssize_t fw_ctrl_size_words_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	size_t nbytes;
	int error;

	mutex_lock(&cs40l26->lock);

	error = cs40l26_sysfs_fw_get_length(cs40l26, &nbytes);

	mutex_unlock(&cs40l26->lock);

	return error ? error : snprintf(buf, PAGE_SIZE, "%zd\n", nbytes / CL_DSP_BYTES_PER_WORD);
}
static DEVICE_ATTR_RO(fw_ctrl_size_words);

static ssize_t fw_ctrl_val_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	char *final_str = NULL;
	u32 reg, *val = NULL;
	char str[CS40L26_FW_CTRL_VAL_STR_SIZE];
	size_t nbytes, num_words;
	unsigned int flags;
	ssize_t nwritten;
	int error, i;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cs40l26_sysfs_fw_get_flags(cs40l26, &flags);
	if (error)
		goto mutex_exit;

	if (!(flags & CL_DSP_HALO_FLAG_READ)) {
		dev_err(cs40l26->dev, "Cannot read from control %s with flags = 0x%X\n",
				cs40l26->sysfs_fw.ctrl_name, flags);
		error = -EPERM;
		cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_SYSFS, __func__);
		goto mutex_exit;
	}

	error = cs40l26_sysfs_fw_get_length(cs40l26, &nbytes);
	if (error)
		goto mutex_exit;

	num_words = nbytes / CL_DSP_BYTES_PER_WORD;

	error = cs40l26_sysfs_fw_get_reg(cs40l26, &reg);
	if (error)
		goto mutex_exit;

	val = kcalloc(num_words, sizeof(u32), GFP_KERNEL);
	if (!val) {
		error = -ENOMEM;
		goto mutex_exit;
	}

	error = regmap_bulk_read(cs40l26->regmap, reg, val, num_words);
	if (error)
		goto mutex_exit;

	final_str = kzalloc(CS40L26_FW_CTRL_VAL_STR_SIZE * num_words, GFP_KERNEL);
	if (!final_str) {
		error = -ENOMEM;
		goto mutex_exit;
	}

	for (i = 0; i < num_words; i++) {
		nwritten = snprintf(str, CS40L26_FW_CTRL_VAL_STR_SIZE, "0x%08X\n", val[i]);
		if (nwritten <= 0) {
			error = -EINVAL;
			goto mutex_exit;
		}

		strncat(final_str, str, CS40L26_FW_CTRL_VAL_STR_SIZE);
	}

mutex_exit:

	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	if (!error)
		nwritten = snprintf(buf, PAGE_SIZE, "%s", final_str);

	kfree(final_str);
	kfree(val);

	return error ? error : nwritten;
}

static ssize_t fw_ctrl_val_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	size_t nbytes, num_buf_words, num_ctl_words, wcount = 0;
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 reg, *val = NULL;
	char *str, *str_full;
	unsigned int flags;
	int error;

	str_full = kstrdup(buf, GFP_KERNEL);
	if (!str_full)
		return -ENOMEM;

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		goto free_exit;

	mutex_lock(&cs40l26->lock);

	error = cs40l26_sysfs_fw_get_flags(cs40l26, &flags);
	if (error)
		goto mutex_exit;

	if (flags & CL_DSP_HALO_FLAG_VOLATILE || !(flags & CL_DSP_HALO_FLAG_WRITE)) {
		dev_err(cs40l26->dev, "Cannot write to control %s with flags = 0x%X\n",
				cs40l26->sysfs_fw.ctrl_name, flags);
		error = -EPERM;
		cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_SYSFS, __func__);
		goto mutex_exit;
	}

	error = cs40l26_sysfs_fw_get_length(cs40l26, &nbytes);
	if (error)
		goto mutex_exit;

	num_ctl_words = nbytes / CL_DSP_BYTES_PER_WORD;

	if (num_ctl_words > 1) {
		num_buf_words = strlen(str_full) / CS40L26_FW_CTRL_VAL_STR_SIZE_NO_NEWLINE;

		if (num_ctl_words != num_buf_words) {
			dev_err(cs40l26->dev, "Expected %zd words, received %zd\n",
					num_ctl_words, num_buf_words);
			error = -EINVAL;
			cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_SYSFS, __func__);
			goto mutex_exit;
		}

		if (strlen(str_full) % CS40L26_FW_CTRL_VAL_STR_SIZE_NO_NEWLINE) {
			dev_err(cs40l26->dev, "Unexpected input string size\n");
			error = -EINVAL;
			cs40l26_log_err(cs40l26, error, CS40L26_ERR_TYPE_SYSFS, __func__);
			goto mutex_exit;
		}
	}

	val = kcalloc(num_ctl_words, sizeof(u32), GFP_KERNEL);
	if (!val) {
		error = -ENOMEM;
		goto mutex_exit;
	}

	while ((str = strsep(&str_full, "\n")) != NULL && wcount < num_ctl_words) {
		error = kstrtou32(str, 16, &val[wcount++]);
		if (error)
			goto mutex_exit;
	}

	error = cs40l26_sysfs_fw_get_reg(cs40l26, &reg);
	if (error)
		goto mutex_exit;

	error = regmap_bulk_write(cs40l26->regmap, reg, val, num_ctl_words);

mutex_exit:
	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

free_exit:
	kfree(val);
	kfree(str_full);

	return error ? error : count;
}
static DEVICE_ATTR_RW(fw_ctrl_val);

static ssize_t fw_mem_block_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	int error;

	mutex_lock(&cs40l26->lock);

	error = snprintf(buf, PAGE_SIZE, "0x%04X\n", cs40l26->sysfs_fw.block_type);

	mutex_unlock(&cs40l26->lock);

	return error;
}

static ssize_t fw_mem_block_type_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 block_type;
	int error;

	error = kstrtou32(buf, 16, &block_type);
	if (error)
		return error;

	switch (block_type) {
	case CL_DSP_XM_UNPACKED_TYPE:
	case CL_DSP_YM_UNPACKED_TYPE:
	case CL_DSP_PM_PACKED_TYPE:
	case CL_DSP_XM_PACKED_TYPE:
	case CL_DSP_YM_PACKED_TYPE:
		break;
	default:
		dev_err(cs40l26->dev, "Invalid block type 0x%X\n", block_type);
		return cs40l26_log_err(cs40l26, -EINVAL, CS40L26_ERR_TYPE_SYSFS, __func__);
	}

	mutex_lock(&cs40l26->lock);

	cs40l26->sysfs_fw.block_type = block_type;

	mutex_unlock(&cs40l26->lock);

	return count;
}
static DEVICE_ATTR_RW(fw_mem_block_type);

static ssize_t rth_latch_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(dev);
	u32 cmd, latch;
	int error;

	if (cs40l26->revid != CS40L26_REVID_A1 && cs40l26->revid != CS40L26_REVID_B1)
		return -EPERM;

	error = kstrtou32(buf, 10, &latch);
	if (error)
		return error;

	switch (latch) {
	case CS40L26_RTH_LATCH_MBOX:
		cmd = CS40L26_DSP_MBOX_CMD_RTH_UPDATE_MBOX;
		break;
	case CS40L26_RTH_LATCH_GPI:
		cmd = CS40L26_DSP_MBOX_CMD_RTH_UPDATE_GPI;
		break;
	default:
		return -EINVAL;
	}

	error = cs40l26_pm_enter(cs40l26->dev);
	if (error)
		return error;

	mutex_lock(&cs40l26->lock);

	error = cs40l26_mailbox_write(cs40l26, cmd);

	mutex_unlock(&cs40l26->lock);

	cs40l26_pm_exit(cs40l26->dev);

	return error ? error : count;
}
static DEVICE_ATTR_WO(rth_latch);

static struct attribute *cs40l26_dev_attrs_fw[] = {
	&dev_attr_fw_algo_id.attr,
	&dev_attr_fw_ctrl_name.attr,
	&dev_attr_fw_ctrl_reg.attr,
	&dev_attr_fw_ctrl_size_words.attr,
	&dev_attr_fw_ctrl_val.attr,
	&dev_attr_fw_mem_block_type.attr,
	&dev_attr_rth_latch.attr,
	NULL,
};

static struct attribute_group cs40l26_dev_attr_fw_group = {
	.name = "firmware",
	.attrs = cs40l26_dev_attrs_fw,
};

const struct attribute_group *cs40l26_attr_groups[] = {
	&cs40l26_dev_attr_group,
	&cs40l26_dev_attr_cal_group,
	&cs40l26_dev_attr_dlog_group,
	&cs40l26_dev_attr_fw_group,
	NULL,
};
