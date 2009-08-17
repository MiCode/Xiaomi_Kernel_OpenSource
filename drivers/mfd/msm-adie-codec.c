/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/mfd/msm-adie-codec.h>
#include <linux/mfd/marimba.h>

static const struct adie_codec_operations *cur_adie_ops;

int adie_codec_register_codec_operations(
			const struct adie_codec_operations *adie_ops)
{
	if (adie_ops == NULL)
		return -EINVAL;

	if (adie_ops->codec_id != adie_get_detected_codec_type())
		return -EINVAL;

	cur_adie_ops = adie_ops;
	pr_info("%s: codec type %d\n", __func__, adie_ops->codec_id);
	return 0;
}

int adie_codec_open(struct adie_codec_dev_profile *profile,
	struct adie_codec_path **path_pptr)
{
	int rc = -EPERM;

	if (cur_adie_ops != NULL) {
		if (cur_adie_ops->codec_open != NULL)
			rc = cur_adie_ops->codec_open(profile, path_pptr);
	} else
		rc = -ENODEV;

	return rc;
}
EXPORT_SYMBOL(adie_codec_open);

int adie_codec_close(struct adie_codec_path *path_ptr)
{
	int rc = -EPERM;

	if (cur_adie_ops != NULL) {
		if (cur_adie_ops->codec_close != NULL)
			rc = cur_adie_ops->codec_close(path_ptr);
	} else
		rc = -ENODEV;

	return rc;
}
EXPORT_SYMBOL(adie_codec_close);

int adie_codec_set_device_digital_volume(struct adie_codec_path *path_ptr,
		u32 num_channels, u32 vol_percentage /* in percentage */)
{
	int rc = -EPERM;

	if (cur_adie_ops != NULL) {
		if (cur_adie_ops->codec_set_device_digital_volume != NULL) {
			rc = cur_adie_ops->codec_set_device_digital_volume(
							path_ptr,
							num_channels,
							vol_percentage);
		}
	} else
		rc = -ENODEV;

	return rc;
}
EXPORT_SYMBOL(adie_codec_set_device_digital_volume);

int adie_codec_set_device_analog_volume(struct adie_codec_path *path_ptr,
		u32 num_channels, u32 volume /* in percentage */)
{
	int rc = -EPERM;

	if (cur_adie_ops != NULL) {
		if (cur_adie_ops->codec_set_device_analog_volume != NULL) {
			rc = cur_adie_ops->codec_set_device_analog_volume(
							path_ptr,
							num_channels,
							volume);
		}
	} else
		rc = -ENODEV;

	return rc;
}
EXPORT_SYMBOL(adie_codec_set_device_analog_volume);

int adie_codec_setpath(struct adie_codec_path *path_ptr, u32 freq_plan, u32 osr)
{
	int rc = -EPERM;

	if (cur_adie_ops != NULL) {
		if (cur_adie_ops->codec_setpath != NULL) {
			rc = cur_adie_ops->codec_setpath(path_ptr,
							freq_plan,
							osr);
		}
	} else
		rc = -ENODEV;

	return rc;
}
EXPORT_SYMBOL(adie_codec_setpath);

u32 adie_codec_freq_supported(struct adie_codec_dev_profile *profile,
	u32 requested_freq)
{
	int rc = -EPERM;

	if (cur_adie_ops != NULL) {
		if (cur_adie_ops->codec_freq_supported != NULL)
			rc = cur_adie_ops->codec_freq_supported(profile,
							requested_freq);
	} else
		rc = -ENODEV;

	return rc;
}
EXPORT_SYMBOL(adie_codec_freq_supported);

int adie_codec_enable_sidetone(struct adie_codec_path *rx_path_ptr,
	u32 enable)
{
	int rc = -EPERM;

	if (cur_adie_ops != NULL) {
		if (cur_adie_ops->codec_enable_sidetone != NULL)
			rc = cur_adie_ops->codec_enable_sidetone(rx_path_ptr,
								enable);
	} else
		rc = -ENODEV;

	return rc;
}
EXPORT_SYMBOL(adie_codec_enable_sidetone);

int adie_codec_enable_anc(struct adie_codec_path *rx_path_ptr,
	u32 enable, struct adie_codec_anc_data *calibration_writes)
{
	int rc = -EPERM;

	if (cur_adie_ops != NULL) {
		if (cur_adie_ops->codec_enable_anc != NULL)
			rc = cur_adie_ops->codec_enable_anc(rx_path_ptr,
				enable, calibration_writes);
	}

	return rc;
}
EXPORT_SYMBOL(adie_codec_enable_anc);

int adie_codec_proceed_stage(struct adie_codec_path *path_ptr, u32 state)
{
	int rc = -EPERM;

	if (cur_adie_ops != NULL) {
		if (cur_adie_ops->codec_proceed_stage != NULL)
			rc = cur_adie_ops->codec_proceed_stage(path_ptr,
								state);
	} else
		rc = -ENODEV;

	return rc;
}
EXPORT_SYMBOL(adie_codec_proceed_stage);

int adie_codec_set_master_mode(struct adie_codec_path *path_ptr, u8 master)
{
	int rc = -EPERM;

	if (cur_adie_ops != NULL) {
		if (cur_adie_ops->codec_set_master_mode != NULL)
			rc = cur_adie_ops->codec_set_master_mode(path_ptr,
					master);
	} else
		rc = -ENODEV;

	return rc;
}
EXPORT_SYMBOL(adie_codec_set_master_mode);


