/*
 * Copyright (C) 2020, SI-IN, Yun Shi (yun.shi@si-in.com).
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


#define DEBUG
#define LOG_FLAG	"sipa_cal_spk"

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include "sipa_common.h"
#include "sipa_timer_task.h"
#include "sipa_tuning_cmd.h"
#include "sipa_parameter.h"
#include "sipa_cal_spk.h"

#define TIMER_TASK_PROC_NAME					("update_spk_calibration")
#define SIPA_SPK_CAL_MAX_CNT					(5)
#define SIPA_SPK_CAL_INTERVAL_TIME_MS			(100)
#define SIPA_SPK_CAL_SET_CAL_TIME_MS			(1000)

static struct sipa_cal_spk_data {
	uint32_t tuning_port_id;
	uint32_t ch;
	int32_t t0;
	int32_t wire_r0;
	uint32_t new_resume;
} spk_data[SIPA_CHANNEL_NUM];

/* 1:valid, other:invalid */
static inline uint32_t is_r0_valid(
	uint32_t ch,
	int32_t r0)
{
	int ret = 0;
	SIPA_EXTRA_CFG cfg;

	ret = sipa_param_read_extra_cfg(ch, &cfg);
	if (0 != ret) {
		pr_err("[  err][%s] %s: sipa_param_read_extra_cfg err %d \r\n",
			LOG_FLAG, __func__, ret);
		return 0;
	}

	if ((r0 > cfg.spk_min_r0) && (r0 < cfg.spk_max_r0))
		return 1;

	return 0;
}

/* 1:valid, other:invalid */
static inline uint32_t is_r0_delta_valid(
	uint32_t ch,
	int32_t delta)
{
	int ret = 0;
	SIPA_EXTRA_CFG cfg;

	ret = sipa_param_read_extra_cfg(ch, &cfg);
	if (0 != ret) {
		pr_err("[  err][%s] %s: sipa_param_read_extra_cfg err %d \r\n",
			LOG_FLAG, __func__, ret);
		return 0;
	}

	/* avoid use abs() */
	if (delta < 0) {
		delta = -delta;
	}

	if (delta < cfg.spk_max_delta_r0)
		return 1;

	return 0;
}

static int sipa_cal_spk_do_cal_task_callback(
	void *data)
{
	int ret = 0;
	struct sipa_cal_spk_data *cal = NULL;
	int32_t cal_cnt = SIPA_SPK_CAL_MAX_CNT, valid_cnt = 0;
	int32_t r0_original = 0, sum_r0 = 0;
	int32_t r0[SIPA_SPK_CAL_MAX_CNT], t0, wire_r0, a;

	cal = data;
	if (NULL == cal) {
		pr_err("[  err][%s] %s: NULL == cal \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	/* get and check reference parameters value */
	ret = sipa_tuning_cmd_get_spk_cal_val(
		cal->tuning_port_id,
		cal->ch,
		&r0_original, &t0, &wire_r0, &a);
	if (0 != ret) {
		pr_err("[  err][%s] %s: get_spk_cal_val ret : %d, "
			"tuning_port_id(%u), ch(%u), "
			"t0(%d), wire_r0(%d) \r\n",
			LOG_FLAG, __func__, ret,
			cal->tuning_port_id,
			cal->ch,
			cal->t0,
			cal->wire_r0);
		return -EEXEC;
	}

	pr_info("[ info][%s] %s: get spker calibrition original val : "
		"tuning_port_id(0x%08x), ch(%u), "
		"r0(%d), t0(%d), wire_r0(%d), a(%d) \r\n",
		LOG_FLAG, __func__, cal->tuning_port_id, cal->ch,
		r0_original, t0, wire_r0, a);

	/* check reference r0 */
	if (1 != is_r0_valid(cal->ch, r0_original)) {
		pr_err("[  err][%s] %s: r0_original(%d) out of r0 range \r\n",
			LOG_FLAG, __func__, r0_original);
		return -EOUTR;
	}

	/* check calibration t0 is valid */
	if (SIPA_CAL_SPK_DEFAULT_VAL == cal->t0) {
		cal->t0 = t0;
	}

	/* check calibration wire_r0 is valid */
	if (SIPA_CAL_SPK_DEFAULT_VAL == cal->wire_r0) {
		cal->wire_r0 = wire_r0;
	}

	pr_info("[ info][%s] %s: calibration val : "
		"t0(%d), wire_r0(%d) \r\n",
		LOG_FLAG, __func__, t0, wire_r0);

	msleep(SIPA_SPK_CAL_INTERVAL_TIME_MS);

	while (cal_cnt > 0) {
		/* execute calibration r0 */
		ret = sipa_tuning_cmd_cal_spk_r0(
			cal->tuning_port_id,
			cal->ch,
			cal->t0,
			cal->wire_r0);
		if (0 != ret) {
			pr_err("[  err][%s] %s: sipa_tuning_cmd_cal_spk_r0 ret : %d, "
				"tuning_port_id(%u), ch(%u), "
				"t0(%d), wire_r0(%d) \r\n",
				LOG_FLAG, __func__, ret,
				cal->tuning_port_id,
				cal->ch,
				cal->t0,
				cal->wire_r0);
			return -EEXEC;
		}
		msleep(SIPA_SPK_CAL_INTERVAL_TIME_MS);

		/* read calibration result */
		ret = sipa_tuning_cmd_get_spk_cal_val(
			cal->tuning_port_id,
			cal->ch,
			&r0[SIPA_SPK_CAL_MAX_CNT - cal_cnt],
			&t0, &wire_r0, &a);
		if (0 != ret) {
			pr_err("[  err][%s] %s: sipa_tuning_cmd_cal_spk_r0 ret : %d, "
				"tuning_port_id(%u), ch(%u), "
				"t0(%d), wire_r0(%d) \r\n",
				LOG_FLAG, __func__, ret,
				cal->tuning_port_id,
				cal->ch,
				cal->t0,
				cal->wire_r0);
			return -EEXEC;
		}
		msleep(SIPA_SPK_CAL_INTERVAL_TIME_MS);

		/* addtion valid r0 */
		if (1 != is_r0_valid(cal->ch, r0[SIPA_SPK_CAL_MAX_CNT - cal_cnt])) {
			pr_warn("[ warn][%s] %s: r0[%d](%d) out of r0 range \r\n",
				LOG_FLAG, __func__,
				SIPA_SPK_CAL_MAX_CNT - cal_cnt,
				r0[SIPA_SPK_CAL_MAX_CNT - cal_cnt]);
		} else {
			sum_r0 += r0[SIPA_SPK_CAL_MAX_CNT - cal_cnt];
			valid_cnt++;
		}

		cal_cnt--;
	}

	if (0 != valid_cnt)
		sum_r0 = sum_r0 / valid_cnt;
	if (1 != is_r0_delta_valid(cal->ch, (sum_r0 - r0_original))) {
		pr_err("[  err][%s] %s: r0 delta out of range, "
			"average r0 %d, valid cnt %d \r\n",
			LOG_FLAG, __func__, sum_r0, valid_cnt);
		ret = sipa_param_write_spk_calibration(
			cal->ch, SIPA_CAL_SPK_FAIL, sum_r0, t0, wire_r0, a);
		return -EOUTR;
	}

	/* write calibration parameters to firmware file */
	ret = sipa_param_write_spk_calibration(
		cal->ch, SIPA_CAL_SPK_OK, sum_r0, t0, wire_r0, a);
	if (0 != ret) {
		pr_err("[  err][%s] %s: write_spk_calibration ret %d \r\n",
			LOG_FLAG, __func__, ret);
		return -EEXEC;
	}

	pr_info("[ info][%s] %s: calibration r0 ok \r\n",
		LOG_FLAG, __func__);

	return 0;
}

static int sipa_cal_spk_update_param_timer_task_callback(
	int is_first,
	void *data)
{
	int ret = 0;
	struct sipa_cal_spk_data *spker;
	SIPA_PARAM_CAL_SPK cal_spk;

	spker = (struct sipa_cal_spk_data *)data;
	if (NULL == spker) {
		pr_err("[  err][%s] %s: NULL == spk_data \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	if (spker->ch >= ARRAY_SIZE(spk_data)) {
		pr_err("[  err][%s] %s: ch(%u) >= ARRAY_SIZE(spk_data) \r\n",
			LOG_FLAG, __func__, spker->ch);
		return -EINVAL;
	}

#if 0
	/* when a new resume coming, download the spker
	 * calibration parameters at the second time */
	if (0 < is_first) {
		spk_data[spker->ch].new_resume = 1;
		is_first = 0;
		return 0;
	}

	if (0 == spk_data[spker->ch].new_resume) {
		return 0;
	}
	spk_data[spker->ch].new_resume = 0;
#endif

	ret = sipa_param_read_spk_calibration(spker->ch, &cal_spk);
	if (0 != ret) {
		pr_err("[  err][%s] %s: sipa_param_read_spk_calibration ret %d \r\n",
			LOG_FLAG, __func__, ret);
		return -EEXEC;
	}

	ret = sipa_tuning_cmd_set_spk_cal_val(spker->tuning_port_id, spker->ch,
		cal_spk.r0, cal_spk.t0, cal_spk.a, cal_spk.wire_r0);
	if (0 != ret) {
		pr_err("[  err][%s] %s: sipa_tuning_cmd_set_spk_cal_val ret %d \r\n",
			LOG_FLAG, __func__, ret);
		return -EEXEC;
	}

	pr_info("[ info][%s] %s: calibration parameter download ok, r0(%d) \r\n",
			LOG_FLAG, __func__, cal_spk.r0);

	return 0;
}

void sipa_cal_spk_execute(
	uint32_t tuning_port_id,
	uint32_t ch,
	int32_t t0,
	int32_t wire_r0)
{

#if 0
	int ret = 0;
	struct task_struct *task = NULL;

	if (ch >= ARRAY_SIZE(spk_data)) {
		pr_err("[  err][%s] %s: ch(%u) >= ARRAY_SIZE(spk_data) \r\n",
			LOG_FLAG, __func__, ch);
		return ;
	}

	spk_data[ch].tuning_port_id = tuning_port_id;
	spk_data[ch].ch = ch;
	spk_data[ch].t0 = t0;
	spk_data[ch].wire_r0 = wire_r0;

	task = kthread_create(
		sipa_cal_spk_do_cal_task_callback,
		&spk_data[ch],
		"sipa_cal_spk_execute");
	if (IS_ERR(task)) {
		pr_err("[  err][%s] %s: kthread_create fail, err code : %ld \r\n",
			LOG_FLAG, __func__, PTR_ERR(task));
		return ;
	}

	ret = wake_up_process(task);
	if (ret < 0) {
		pr_err("[  err][%s] %s: wake_up_process fail, err code : %d \r\n",
			LOG_FLAG, __func__, ret);
		return ;
	}
#else
	if (ch >= ARRAY_SIZE(spk_data)) {
		pr_err("[  err][%s] %s: ch(%u) >= ARRAY_SIZE(spk_data) \r\n",
			LOG_FLAG, __func__, ch);
		return ;
	}

	spk_data[ch].tuning_port_id = tuning_port_id;
	spk_data[ch].ch = ch;
	spk_data[ch].t0 = t0;
	spk_data[ch].wire_r0 = wire_r0;

	sipa_cal_spk_do_cal_task_callback(&spk_data[ch]);
#endif

	return ;
}

void sipa_cal_spk_update_probe(
	uint32_t timer_task_hdl,
	uint32_t tuning_port_id,
	uint32_t ch)
{
	int ret = 0;

	if (ch >= ARRAY_SIZE(spk_data)) {
		pr_err("[  err][%s] %s: ch(%u) >= ARRAY_SIZE(spk_data) \r\n",
			LOG_FLAG, __func__, ch);
		return ;
	}

	spk_data[ch].tuning_port_id = tuning_port_id;
	spk_data[ch].ch = ch;
	// spk_data[ch].new_resume = 0;

	ret = sipa_timer_task_register(
		timer_task_hdl,
		TIMER_TASK_PROC_NAME,
		sipa_timer_usr_id_spk_cal(ch),
		SIPA_SPK_CAL_SET_CAL_TIME_MS,
		/* is_first > 0 means first, other means not first */
		sipa_cal_spk_update_param_timer_task_callback,
		&spk_data[ch],
		1,
		0);

	if (0 > ret) {
		pr_err("[  err][%s] %s: sipa_timer_task_register ret : %d \r\n",
			LOG_FLAG, __func__, ret);
	}

	pr_info("[ info][%s] %s: done, ret(%d) \r\n",
			LOG_FLAG, __func__, ret);

	return ;
}

void sipa_cal_spk_update_remove(
	uint32_t timer_task_hdl,
	uint32_t ch)
{
	int ret = sipa_timer_task_unregister(
		timer_task_hdl,
		sipa_timer_usr_id_spk_cal(ch));
	if (0 != ret) {
		pr_err("[  err][%s] %s: sipa_timer_task_unregister ret : %d \r\n",
			LOG_FLAG, __func__, ret);
	}

	return ;
}
