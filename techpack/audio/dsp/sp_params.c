// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <dsp/q6audio-v2.h>
#include <dsp/q6afe-v2.h>
#include <audio/linux/msm_audio_calibration.h>
#include <dsp/sp_params.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>

/* export or show spk params at /sys/class/spk_params/cal_data */
#define SPK_PARAMS  "spk_params"
#define CLASS_NAME "cal_data"
#define BUF_SZ 20
#define Q22 (1<<22)
#define Q13 (1<<13)
#define Q7 (1<<7)

struct afe_spk_ctl {
	struct class *p_class;
	struct device *p_dev;
	struct afe_sp_rx_tmax_xmax_logging_param xt_logging;
	int32_t max_temperature_rd[SP_V2_NUM_MAX_SPKR];
};
struct afe_spk_ctl this_afe_spk;

static ssize_t sp_count_exceeded_temperature_l_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	ssize_t ret = 0;

	ret = snprintf(buf, BUF_SZ, "%d\n",
	this_afe_spk.xt_logging.count_exceeded_temperature[SP_V2_SPKR_1]);
	this_afe_spk.xt_logging.count_exceeded_temperature[SP_V2_SPKR_1] = 0;
	return ret;
}
static DEVICE_ATTR(count_exceeded_temperature, 0644,
		sp_count_exceeded_temperature_l_show, NULL);

static ssize_t sp_count_exceeded_temperature_r_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	ssize_t ret = 0;

	ret = snprintf(buf, BUF_SZ, "%d\n",
	this_afe_spk.xt_logging.count_exceeded_temperature[SP_V2_SPKR_2]);
	this_afe_spk.xt_logging.count_exceeded_temperature[SP_V2_SPKR_2] = 0;
	return ret;
}
static DEVICE_ATTR(count_exceeded_temperature_r, 0644,
		sp_count_exceeded_temperature_r_show, NULL);

static ssize_t sp_count_exceeded_excursion_l_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	ssize_t ret = 0;

	ret = snprintf(buf, BUF_SZ, "%d\n",
		this_afe_spk.xt_logging.count_exceeded_excursion[SP_V2_SPKR_1]);
	this_afe_spk.xt_logging.count_exceeded_excursion[SP_V2_SPKR_1] = 0;
	return ret;
}
static DEVICE_ATTR(count_exceeded_excursion, 0644,
		sp_count_exceeded_excursion_l_show, NULL);

static ssize_t sp_count_exceeded_excursion_r_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t ret = 0;

	ret = snprintf(buf, BUF_SZ, "%d\n",
		this_afe_spk.xt_logging.count_exceeded_excursion[SP_V2_SPKR_2]);
	this_afe_spk.xt_logging.count_exceeded_excursion[SP_V2_SPKR_2] = 0;
	return ret;
}
static DEVICE_ATTR(count_exceeded_excursion_r, 0644,
		sp_count_exceeded_excursion_r_show, NULL);

static ssize_t sp_max_excursion_l_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	ssize_t ret = 0;
	int32_t ex_val_frac;
	int32_t ex_q27 = this_afe_spk.xt_logging.max_excursion[SP_V2_SPKR_1];

	ex_val_frac = ex_q27/Q13;
	ex_val_frac = (ex_val_frac * 10000)/(Q7 * Q7);
	ex_val_frac /= 100;
	ret = snprintf(buf, BUF_SZ, "%d.%02d\n", 0, ex_val_frac);
	this_afe_spk.xt_logging.max_excursion[SP_V2_SPKR_1] = 0;
	return ret;
}
static DEVICE_ATTR(max_excursion, 0644, sp_max_excursion_l_show, NULL);

static ssize_t sp_max_excursion_r_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	ssize_t ret = 0;
	int32_t ex_val_frac;
	int32_t ex_q27 = this_afe_spk.xt_logging.max_excursion[SP_V2_SPKR_2];

	ex_val_frac = ex_q27/Q13;
	ex_val_frac = (ex_val_frac * 10000)/(Q7 * Q7);
	ex_val_frac /= 100;
	ret = snprintf(buf, BUF_SZ, "%d.%02d\n", 0, ex_val_frac);
	this_afe_spk.xt_logging.max_excursion[SP_V2_SPKR_2] = 0;
	return ret;
}
static DEVICE_ATTR(max_excursion_r, 0644, sp_max_excursion_r_show, NULL);

static ssize_t sp_max_temperature_l_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	ssize_t ret = 0;

	ret = snprintf(buf, BUF_SZ, "%d\n",
		this_afe_spk.xt_logging.max_temperature[SP_V2_SPKR_1]/Q22);
	this_afe_spk.xt_logging.max_temperature[SP_V2_SPKR_1] = 0;
	return ret;
}
static DEVICE_ATTR(max_temperature, 0644, sp_max_temperature_l_show, NULL);

static ssize_t sp_max_temperature_r_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	ssize_t ret = 0;

	ret = snprintf(buf, BUF_SZ, "%d\n",
		this_afe_spk.xt_logging.max_temperature[SP_V2_SPKR_2]/Q22);
	this_afe_spk.xt_logging.max_temperature[SP_V2_SPKR_2] = 0;
	return ret;
}
static DEVICE_ATTR(max_temperature_r, 0644, sp_max_temperature_r_show, NULL);

static ssize_t sp_max_temperature_rd_l_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return snprintf(buf, BUF_SZ, "%d\n",
			this_afe_spk.max_temperature_rd[SP_V2_SPKR_1]/Q22);
}
static DEVICE_ATTR(max_temperature_rd, 0644,
		sp_max_temperature_rd_l_show, NULL);

static ssize_t sp_max_temperature_rd_r_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return snprintf(buf, BUF_SZ, "%d\n",
			this_afe_spk.max_temperature_rd[SP_V2_SPKR_2]/Q22);
}
static DEVICE_ATTR(max_temperature_rd_r, 0644,
		sp_max_temperature_rd_r_show, NULL);

static ssize_t q6afe_initial_cal_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	return snprintf(buf, BUF_SZ, "%d\n", afe_get_spk_initial_cal());
}

static ssize_t q6afe_initial_cal_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	int initial_cal = 0;

	if (!kstrtou32(buf, 0, &initial_cal)) {
		initial_cal = initial_cal > 0 ? 1 : 0;
		if (initial_cal == afe_get_spk_initial_cal())
			dev_dbg(dev, "%s: same value already present\n",
				__func__);
		else
			afe_set_spk_initial_cal(initial_cal);
	}
	return size;
}

static DEVICE_ATTR(initial_cal, 0644,
	q6afe_initial_cal_show, q6afe_initial_cal_store);

static ssize_t q6afe_v_vali_flag_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	return snprintf(buf, BUF_SZ, "%d\n", afe_get_spk_v_vali_flag());
}

static ssize_t q6afe_v_vali_flag_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	int v_vali_flag = 0;

	if (!kstrtou32(buf, 0, &v_vali_flag)) {
		v_vali_flag = v_vali_flag > 0 ? 1 : 0;
		if (v_vali_flag == afe_get_spk_v_vali_flag())
			dev_dbg(dev, "%s: same value already present\n",
				__func__);
		else
			afe_set_spk_v_vali_flag(v_vali_flag);
	}
	return size;
}

static DEVICE_ATTR(v_vali_flag, 0644,
	q6afe_v_vali_flag_show, q6afe_v_vali_flag_store);

static ssize_t q6afe_spk_r0_l_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	int r0[SP_V2_NUM_MAX_SPKRS];

	afe_get_spk_r0(r0);
	return snprintf(buf, BUF_SZ, "%d\n", r0[SP_V2_SPKR_1]);
}

static DEVICE_ATTR(spk_r0, 0644, q6afe_spk_r0_l_show, NULL);

static ssize_t q6afe_spk_t0_l_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	int t0[SP_V2_NUM_MAX_SPKRS];

	afe_get_spk_t0(t0);
	return snprintf(buf, BUF_SZ, "%d\n", t0[SP_V2_SPKR_1]);
}

static DEVICE_ATTR(spk_t0, 0644, q6afe_spk_t0_l_show, NULL);

static ssize_t q6afe_spk_r0_r_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int r0[SP_V2_NUM_MAX_SPKRS];

	afe_get_spk_r0(r0);
	return snprintf(buf, BUF_SZ, "%d\n", r0[SP_V2_SPKR_2]);
}

static DEVICE_ATTR(spk_r0_r, 0644, q6afe_spk_r0_r_show, NULL);

static ssize_t q6afe_spk_t0_r_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int t0[SP_V2_NUM_MAX_SPKRS];

	afe_get_spk_t0(t0);
	return snprintf(buf, BUF_SZ, "%d\n", t0[SP_V2_SPKR_2]);
}

static DEVICE_ATTR(spk_t0_r, 0644, q6afe_spk_t0_r_show, NULL);

static ssize_t q6afe_spk_v_vali_l_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int v_vali_sts[SP_V2_NUM_MAX_SPKRS];

	afe_get_spk_v_vali_sts(v_vali_sts);
	return snprintf(buf, BUF_SZ, "%d\n", v_vali_sts[SP_V2_SPKR_1]);
}

static DEVICE_ATTR(spk_v_vali_status, 0644, q6afe_spk_v_vali_l_show, NULL);

static ssize_t q6afe_spk_v_vali_r_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	int v_vali_sts[SP_V2_NUM_MAX_SPKRS];

	afe_get_spk_v_vali_sts(v_vali_sts);
	return snprintf(buf, BUF_SZ, "%d\n", v_vali_sts[SP_V2_SPKR_2]);
}

static DEVICE_ATTR(spk_v_vali_r_status, 0644, q6afe_spk_v_vali_r_show, NULL);

static struct attribute *afe_spk_cal_attr[] = {
	&dev_attr_max_excursion.attr,
	&dev_attr_max_excursion_r.attr,
	&dev_attr_max_temperature.attr,
	&dev_attr_max_temperature_r.attr,
	&dev_attr_count_exceeded_excursion.attr,
	&dev_attr_count_exceeded_excursion_r.attr,
	&dev_attr_count_exceeded_temperature.attr,
	&dev_attr_count_exceeded_temperature_r.attr,
	&dev_attr_max_temperature_rd.attr,
	&dev_attr_max_temperature_rd_r.attr,
	&dev_attr_initial_cal.attr,
	&dev_attr_spk_r0.attr,
	&dev_attr_spk_t0.attr,
	&dev_attr_spk_r0_r.attr,
	&dev_attr_spk_t0_r.attr,
	&dev_attr_v_vali_flag.attr,
	&dev_attr_spk_v_vali_status.attr,
	&dev_attr_spk_v_vali_r_status.attr,
	NULL,
};

static struct attribute_group afe_spk_cal_attr_grp = {
	.attrs = afe_spk_cal_attr,
};


/**
 * afe_get_sp_xt_logging_data -
 *       to get excursion logging data from DSP
 *
 * @port: AFE port ID
 *
 * Returns 0 on success or error on failure
 */
int afe_get_sp_xt_logging_data(u16 port_id)
{
	int ret = 0;
	struct afe_sp_rx_tmax_xmax_logging_param xt_logging_data;

	ret = afe_get_sp_rx_tmax_xmax_logging_data(&xt_logging_data, port_id);
	if (ret) {
		pr_err("%s Excursion logging fail\n", __func__);
		return ret;
	}
	/* storing max sp param value */
	if (this_afe_spk.xt_logging.max_temperature[SP_V2_SPKR_1] <
		xt_logging_data.max_temperature[SP_V2_SPKR_1])
		this_afe_spk.xt_logging.max_temperature[SP_V2_SPKR_1] =
				xt_logging_data.max_temperature[SP_V2_SPKR_1];


	if (this_afe_spk.xt_logging.max_temperature[SP_V2_SPKR_2] <
		xt_logging_data.max_temperature[SP_V2_SPKR_2])
		this_afe_spk.xt_logging.max_temperature[SP_V2_SPKR_2] =
				xt_logging_data.max_temperature[SP_V2_SPKR_2];


	/* update temp for max_temperature_rd node */
	if (this_afe_spk.max_temperature_rd[SP_V2_SPKR_1] <
		xt_logging_data.max_temperature[SP_V2_SPKR_1])
		this_afe_spk.max_temperature_rd[SP_V2_SPKR_1] =
				xt_logging_data.max_temperature[SP_V2_SPKR_1];

	if (this_afe_spk.max_temperature_rd[SP_V2_SPKR_2] <
		xt_logging_data.max_temperature[SP_V2_SPKR_2])
		this_afe_spk.max_temperature_rd[SP_V2_SPKR_2] =
				xt_logging_data.max_temperature[SP_V2_SPKR_2];


	if (this_afe_spk.xt_logging.max_excursion[SP_V2_SPKR_1] <
		xt_logging_data.max_excursion[SP_V2_SPKR_1])
		this_afe_spk.xt_logging.max_excursion[SP_V2_SPKR_1] =
				xt_logging_data.max_excursion[SP_V2_SPKR_1];

	if (this_afe_spk.xt_logging.max_excursion[SP_V2_SPKR_2] <
		xt_logging_data.max_excursion[SP_V2_SPKR_2])
		this_afe_spk.xt_logging.max_excursion[SP_V2_SPKR_2] =
				xt_logging_data.max_excursion[SP_V2_SPKR_2];

	if (this_afe_spk.xt_logging.count_exceeded_temperature[SP_V2_SPKR_1] <
		xt_logging_data.count_exceeded_temperature[SP_V2_SPKR_1])
		this_afe_spk.xt_logging.count_exceeded_temperature[SP_V2_SPKR_1]
		+= xt_logging_data.count_exceeded_temperature[SP_V2_SPKR_1];

	if (this_afe_spk.xt_logging.count_exceeded_temperature[SP_V2_SPKR_2] <
		xt_logging_data.count_exceeded_temperature[SP_V2_SPKR_2])
		this_afe_spk.xt_logging.count_exceeded_temperature[SP_V2_SPKR_2]
		+= xt_logging_data.count_exceeded_temperature[SP_V2_SPKR_2];

	if (this_afe_spk.xt_logging.count_exceeded_excursion[SP_V2_SPKR_1] <
		xt_logging_data.count_exceeded_excursion[SP_V2_SPKR_1])
		this_afe_spk.xt_logging.count_exceeded_excursion[SP_V2_SPKR_1]
		+= xt_logging_data.count_exceeded_excursion[SP_V2_SPKR_1];

	if (this_afe_spk.xt_logging.count_exceeded_excursion[SP_V2_SPKR_2] <
		xt_logging_data.count_exceeded_excursion[SP_V2_SPKR_2])
		this_afe_spk.xt_logging.count_exceeded_excursion[SP_V2_SPKR_2]
		+= xt_logging_data.count_exceeded_excursion[SP_V2_SPKR_2];

	return ret;
}
EXPORT_SYMBOL(afe_get_sp_xt_logging_data);

int __init spk_params_init(void)
{
	/* initialize xt param value with 0 */
	this_afe_spk.xt_logging.max_temperature[SP_V2_SPKR_1] = 0;
	this_afe_spk.xt_logging.max_temperature[SP_V2_SPKR_2] = 0;
	this_afe_spk.xt_logging.max_excursion[SP_V2_SPKR_1] = 0;
	this_afe_spk.xt_logging.max_excursion[SP_V2_SPKR_2] = 0;
	this_afe_spk.xt_logging.count_exceeded_temperature[SP_V2_SPKR_1] = 0;
	this_afe_spk.xt_logging.count_exceeded_temperature[SP_V2_SPKR_2] = 0;
	this_afe_spk.xt_logging.count_exceeded_excursion[SP_V2_SPKR_1] = 0;
	this_afe_spk.xt_logging.count_exceeded_excursion[SP_V2_SPKR_2] = 0;

	this_afe_spk.p_class = class_create(THIS_MODULE, SPK_PARAMS);
	if (this_afe_spk.p_class) {
		this_afe_spk.p_dev = device_create(this_afe_spk.p_class, NULL,
						   1, NULL, CLASS_NAME);
		if (!IS_ERR(this_afe_spk.p_dev)) {
			if (sysfs_create_group(&this_afe_spk.p_dev->kobj,
				&afe_spk_cal_attr_grp))
				pr_err("%s: Failed to create sysfs group\n",
					__func__);
		}
	}
	return 0;
}

void spk_params_exit(void)
{
	pr_debug("%s\n", __func__);
}
