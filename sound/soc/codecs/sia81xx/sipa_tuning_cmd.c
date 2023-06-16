/*
 * Copyright (C) 2020 SI-IN, Yun Shi (yun.shi@si-in.com).
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
#define LOG_FLAG	"sipa_tuning_cmd"

#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/errno.h>
//#include <linux/types.h>
#include "sipa_common.h"
#include "sipa_tuning_if.h"
#include "sipa_tuning_cmd.h"
#include "sipa_cal_spk.h"
#include "sipa_timer_task.h"
#include "sipa_parameter_typedef.h"
#include "sipa_parameter.h"


#define SIXTH_SIPA_RX_MODULE						(0x1000E900)/* module id */
#define SIXTH_SIPA_RX_ENABLE						(0x1000EA01)/* parameter id */
#define SIXTH_SIPA_RX_CORE_TOPO						(0x1000EA02)/* parameter id */
#define SIXTH_SIPA_RX_CORE_PARAM					(0x1000EA03)/* parameter id */

#define SIXTH_SIPA_TX_MODULE						(0x1000F900)/* module id */
#define SIXTH_SIPA_TX_ENABLE						(0x1000FA01)/* parameter id */
#define SIXTH_SIPA_TX_CORE_TOPO						(0x1000FA02)/* parameter id */
#define SIXTH_SIPA_TX_CORE_PARAM					(0x1000FA03)/* parameter id */

#define TUNING_MSG_INVALID_VAL						(0xFFFFFFFF)
#define MAX_TUNING_MSG_LEN							(1024)

#define ID_VOL_MSG								    (62)
#define ID_MONITOR_MSG								(65)
#define ID_VDD_MSG								    (66)
#define ID_DEBUG_SH_MSG								(68)
#define ID_SPEAKER_MSG								(73)
#define ID_HOC_MSG								    (62)
#define ID_TEMP_LIMITER                             (72)
#define ID_F0_TRACKING								(74)

#define CAL_TYPE_CAL_R0								(2)
#define CAL_TYPE_CAL_A								(3)
#define CAL_TYPE_SET_CAL							(4)
#define CAL_TYPE_SET_SPK_MODEL						(5)
#define CAL_TYPE_GET_SPK_MODEL						(131)

#define CAL_TYPE_GET_CAL							(0x81)
#define CAL_TYPE_GET_F0								(0x82)
#define CAL_TYPE_GET_STATE							(0x82)

#define SIPA_SPK_CAL_R0_TIME_MS				(1000)
#define SIPA_SET_SPK_MODEL_TIME_MS			(1000)
#define SIPA_CLOSE_F0_TEMP_TIME_MS			(1000)
#define SIPA_GET_SPK_MODEL_TIME_MS			(30000)
#define TIMER_TASK_PROC_CAL_R0				("auto_cal_r0")
#define TIMER_TASK_PROC_SET_SPK_MODEL		("auto_set_spk_model")
#define TIMER_TASK_PROC_GET_SPK_MODEL		("auto_get_spk_model")
#define TIMER_TASK_PROC_CLOSE_F0_TEMP		("auto_close_f0_temp")


typedef struct sipa_tuning_param_head {
	uint32_t proc_code;
	uint32_t id;
	uint32_t msg_len;
	char msg[];
} __packed SIPA_TUNING_PARAM_HEAD;

typedef struct sipa_temp_limiter_msg {
	uint32_t on;
	uint32_t arr[14];
} __packed TEMP_LIMITER_MSG;

typedef struct sipa_f0_tracking_msg {
	uint32_t on;
	uint32_t arr[19];
} __packed F0_TRACKING_MSG;


typedef struct sipa_vdd_msg {
	uint32_t vdd; //uv
	uint32_t res[5];
} __packed SIPA_VDD_MSG;

typedef struct __ad_hoc_msg_{
	uint32_t ad_hoc;
} __packed AD_HOC_MSG;

static struct sipa_cal_r0{
	uint32_t tuning_port_id;
	uint32_t ch;
	uint32_t ad_hoc;
} cal_r0[SIPA_CHANNEL_NUM];

static struct sipa_spk_model{
	uint32_t tuning_port_id;
	uint32_t ch;
	SIPA_PARAM_SPK_MODEL_PARAM spk_model;
} spk_model_param[SIPA_CHANNEL_NUM];

static struct sipa_f0_temp_module{
	uint32_t timer_task_hdl;
	uint32_t tuning_port_id;
	uint32_t ch;
} sipa_f0_temp[SIPA_CHANNEL_NUM];

typedef struct sia81xx_calibration_msg {
	uint32_t msg_type;
	union {
		int32_t res[8];
		struct {
			int32_t t0;
			int32_t wire_r0;
		} calibration_r0;
		struct {
			int32_t t;
		} calibration_a;
		struct {
			int32_t r0;
			int32_t t0;
			int32_t wire_r0;
			int32_t a;
		} calibrated_param;
		struct {
			int32_t f0;
			int32_t q;
			int32_t xthresh;
			int32_t xthresh_rdc;
		} spk_model_param;
		struct {
			int32_t instant_f0;/* set instant f0 at first element for compatible with SPK_MSG_TYPE_GET_F0 */
			int32_t rdc;
			int32_t temperature;
		} spk_state;

	};
} __packed SIA81XX_CALIBRATION_MSG;

typedef struct sia81xx_monitor_data_msg {
	int32_t res0[10];
	float vdd;
	int32_t res1;
	struct {
		float temperature;
		float r0;
		float a;
		float f0;
		float q;
		float t0;
		float wire_r0;
		float xthresh;
	} spker;
	int32_t res2[128];
} __packed SIA81XX_MONITOR_DATA_MSG;

typedef struct sia81xx_vol_msg_{
	float vol;
} __packed SIA81XX_VOL_MSG;

typedef struct sia81xx_debug_show {
	uint32_t proc_code[17];
} __packed SIA81XX_DEBUG_SHOW;

static inline int32_t sipa_float_to_int(void *f, uint32_t mul)
{
	uint32_t raw = *((uint32_t *)f);
	uint8_t s = ((raw >> 31) & 0x00000001);
	uint8_t e = ((raw >> 23) & 0x000000FF);
	uint32_t b = ((raw & 0x00FFFFFF) | 0x00800000);

	e = e - 127 + mul;
	e = (24 - e - 1);
	if ((e >= 24) || (e <= -24))
		return 0;

	if (e >= 0) {
		if (1 == s)
			return -1 * (b >> e);
		else
			return (b >> e);
	} else {
		if (1 == s)
			return -1 * (b << e);
		else
			return (b << e);
	}
}

static int sipa_tuning_cmd_set(
	uint32_t cal_id,
	uint32_t mode_id,
	uint32_t param_id,
	uint32_t size,
	uint8_t *payload)
{
	int ret = 0;
	unsigned long cal_handle = 0;

	if ((NULL == tuning_if_opt.write) || (NULL == tuning_if_opt.open)) {
		pr_err("[  err][%s] %s: write(%p), open(%p) \r\n",
			LOG_FLAG, __func__, tuning_if_opt.write, tuning_if_opt.open);
		return -EINVAL;
	}

	cal_handle = tuning_if_opt.open(cal_id);
	if (0 == cal_handle) {
		pr_err("[  err][%s] %s: tuning_if_opt.open failed \r\n",
			LOG_FLAG, __func__);
		return -EEXEC;
	}

	ret = tuning_if_opt.write(
		cal_handle,
		mode_id,
		param_id,
		size,
		payload);
	if (0 > ret) {
		pr_err("[  err][%s] %s: tuning_if_opt.write failed ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		return -EEXEC;
	}

	return 0;
}

static int sipa_tuning_cmd_get(
	uint32_t cal_id,
	uint32_t mode_id,
	uint32_t param_id,
	uint32_t size,
	uint8_t *payload)
{
	int ret = 0;
	unsigned long cal_handle = 0;

	if ((NULL == tuning_if_opt.read) || (NULL == tuning_if_opt.open)) {
		pr_err("[  err][%s] %s: read(%p), open(%p) \r\n",
			LOG_FLAG, __func__, tuning_if_opt.read, tuning_if_opt.open);
		return -EINVAL;
	}

	cal_handle = tuning_if_opt.open(cal_id);
	if (0 == cal_handle) {
		pr_err("[  err][%s] %s: tuning_if_opt.open failed \r\n",
			LOG_FLAG, __func__);
		return -EEXEC;
	}

	ret = tuning_if_opt.read(
		cal_handle,
		mode_id,
		param_id,
		size,
		payload);
	if (ret > size) {
		pr_err("[  err][%s] %s: tuning_if_opt.read failed "
			"ret = %d, size = %u \r\n",
			LOG_FLAG, __func__, ret, size);
		return -EEXEC;
	}

	return 0;
}

int sipa_tuning_cmd_set_en(
	uint32_t cal_id,
	uint32_t en)
{
	int ret = 0;

	ret = sipa_tuning_cmd_set(
		cal_id,
		SIXTH_SIPA_RX_MODULE,
		SIXTH_SIPA_RX_ENABLE,
		sizeof(en),
		(uint8_t *)&en);
	if (0 > ret) {
		pr_err("[  err][%s] %s: sipa_tuning_cmd_set failed "
			"ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		return -EEXEC;
	}

	return 0;
}

int sipa_tuning_cmd_get_en(
	uint32_t cal_id)
{
	int ret = 0;
	uint32_t enable = 0xFF;

	ret = sipa_tuning_cmd_get(
		cal_id,
		SIXTH_SIPA_RX_MODULE,
		SIXTH_SIPA_RX_ENABLE,
		sizeof(enable),
		(uint8_t *)&enable);
	if (0 > ret) {
		pr_err("[  err][%s] %s: sipa_tuning_cmd_get failed "
			"ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		return -EEXEC;
	} else {
		pr_info("[ info][%s] %s: tuning enable = %u : \r\n",
			LOG_FLAG, __func__, enable);
	}

	return enable;
}


int sipa_tuning_cmd_set_vdd(
	uint32_t cal_id,
	uint32_t ch,
	uint32_t vdd)
{
	int ret = 0;
	uint8_t buf[MAX_TUNING_MSG_LEN];
	SIPA_TUNING_PARAM_HEAD *head = (SIPA_TUNING_PARAM_HEAD *)buf;
	SIPA_VDD_MSG *msg = (SIPA_VDD_MSG *)(head->msg);
	uint32_t cmd_len =
		sizeof(SIPA_TUNING_PARAM_HEAD) + sizeof(SIPA_VDD_MSG);

	memset(buf, 0, sizeof(buf));

	head->id = ID_VDD_MSG;
	head->msg_len = sizeof(SIPA_VDD_MSG);
	head->proc_code = ch;// ch sn

	msg->vdd = vdd;
	/* don't set these in driver, it should be setted at acdb and it's fixed */
	msg->res[0] = TUNING_MSG_INVALID_VAL;
	msg->res[1] = TUNING_MSG_INVALID_VAL;
	msg->res[2] = TUNING_MSG_INVALID_VAL;
	msg->res[3] = TUNING_MSG_INVALID_VAL;
	msg->res[4] = TUNING_MSG_INVALID_VAL;

	ret = sipa_tuning_cmd_set(
		cal_id,
		SIXTH_SIPA_RX_MODULE,
		SIXTH_SIPA_RX_CORE_PARAM,
		cmd_len,
		buf);
	if (0 > ret) {
		pr_err("[  err][%s] %s: sipa_tuning_cmd_set failed ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		return -EEXEC;
	} else {
		pr_info("[ info][%s] %s: vdd = %u : ", LOG_FLAG, __func__, vdd);
	}

	return 0;
}

int sipa_tuning_cmd_print_monitor_data(
	uint32_t cal_id,
	uint32_t ch)
{
	int ret = 0;
	uint8_t buf[MAX_TUNING_MSG_LEN];
	SIPA_TUNING_PARAM_HEAD *head = (SIPA_TUNING_PARAM_HEAD *)buf;
	SIA81XX_MONITOR_DATA_MSG *msg = (SIA81XX_MONITOR_DATA_MSG *)(head->msg);
	uint32_t cmd_len =
		sizeof(SIPA_TUNING_PARAM_HEAD) + sizeof(SIA81XX_MONITOR_DATA_MSG);

	memset(buf, 0, sizeof(buf));

	head->id = ID_MONITOR_MSG;
	head->msg_len = sizeof(SIA81XX_MONITOR_DATA_MSG);
	head->proc_code = ch;// ch sn

	ret = sipa_tuning_cmd_get(
		cal_id,
		SIXTH_SIPA_RX_MODULE,
		SIXTH_SIPA_RX_CORE_PARAM,
		cmd_len,
		buf);
	if (0 > ret) {
		pr_err("[  err][%s] %s: sipa_tuning_cmd_get failed "
			"ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		return -EEXEC;
	}

	pr_info("[ info][%s] %s: monitor data : "
		"vdd(%d/1024), temperature(%d/1024), r0(%d/1024), "
		"a(%d/1048576), f0(%d/1024), q(%d/1024), "
		"t0(%d/1024), wire_r0(%d/1024), xthresh(%d/1024),  \r\n",
		LOG_FLAG, __func__,
		sipa_float_to_int(&msg->vdd, 10),
		sipa_float_to_int(&msg->spker.temperature, 10),
		sipa_float_to_int(&msg->spker.r0, 10),
		sipa_float_to_int(&msg->spker.a, 20),
		sipa_float_to_int(&msg->spker.f0, 10),
		sipa_float_to_int(&msg->spker.q, 10),
		sipa_float_to_int(&msg->spker.t0, 10),
		sipa_float_to_int(&msg->spker.wire_r0, 10),
		sipa_float_to_int(&msg->spker.xthresh, 10));

	return 0;
}

int sipa_tuning_cmd_set_volume(
	uint32_t cal_id,
	uint32_t ch,
	int32_t vol)
{
	int ret = 0;
	uint8_t buf[MAX_TUNING_MSG_LEN];
	SIPA_TUNING_PARAM_HEAD *head = (SIPA_TUNING_PARAM_HEAD *)buf;
	SIA81XX_VOL_MSG *msg = (SIA81XX_VOL_MSG *)(head->msg);
	uint32_t cmd_len =
		sizeof(SIPA_TUNING_PARAM_HEAD) + sizeof(SIA81XX_VOL_MSG);

	memset(buf, 0, sizeof(buf));

	head->id = ID_VOL_MSG;
	head->msg_len = sizeof(SIA81XX_VOL_MSG);
	head->proc_code = ch;// ch sn

	memcpy((void *)&msg->vol, (void *)&vol, sizeof(msg->vol));

	ret = sipa_tuning_cmd_set(
		cal_id,
		SIXTH_SIPA_RX_MODULE,
		SIXTH_SIPA_RX_CORE_PARAM,
		cmd_len,
		buf);

	if (0 > ret) {
		pr_err("[  err][%s] %s: sipa_tuning_cmd_set failed "
			"ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		return -EEXEC;
	}

	return 0;
}

int sipa_tuning_cmd_set_spk_cal_val(
	uint32_t cal_id,
	uint32_t ch,
	int32_t r0,
	int32_t t0,
	int32_t a,
	int32_t wire_r0)
{
	int ret = 0;
	uint8_t buf[MAX_TUNING_MSG_LEN];
	SIPA_TUNING_PARAM_HEAD *head = (SIPA_TUNING_PARAM_HEAD *)buf;
	SIA81XX_CALIBRATION_MSG *msg = (SIA81XX_CALIBRATION_MSG *)(head->msg);
	uint32_t cmd_len =
		sizeof(SIPA_TUNING_PARAM_HEAD) + sizeof(SIA81XX_CALIBRATION_MSG);

	memset(buf, 0, sizeof(buf));

	head->id = ID_SPEAKER_MSG;
	head->msg_len = sizeof(SIA81XX_CALIBRATION_MSG);
	head->proc_code = ch;// ch sn

	msg->msg_type = CAL_TYPE_SET_CAL;
	msg->calibrated_param.r0 = r0;
	msg->calibrated_param.t0 = t0;
	msg->calibrated_param.wire_r0 = wire_r0;
	msg->calibrated_param.a = a;

	ret = sipa_tuning_cmd_set(
		cal_id,
		SIXTH_SIPA_RX_MODULE,
		SIXTH_SIPA_RX_CORE_PARAM,
		cmd_len,
		buf);
	if (0 > ret) {
		pr_err("[  err][%s] %s: sipa_tuning_cmd_set failed "
			"ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		return -EEXEC;
	}

	return 0;
}

int sipa_tuning_cmd_get_spk_cal_val(
	uint32_t cal_id,
	uint32_t ch,
	int32_t *r0,
	int32_t *t0,
	int32_t *wire_r0,
	int32_t *a)
{
	int ret = 0;
	uint8_t buf[MAX_TUNING_MSG_LEN];
	SIPA_TUNING_PARAM_HEAD *head = (SIPA_TUNING_PARAM_HEAD *)buf;
	SIA81XX_CALIBRATION_MSG *msg = (SIA81XX_CALIBRATION_MSG *)(head->msg);
	uint32_t cmd_len =
		sizeof(SIPA_TUNING_PARAM_HEAD) + sizeof(SIA81XX_CALIBRATION_MSG);

	memset(buf, 0, sizeof(buf));

	head->id = ID_SPEAKER_MSG;
	head->msg_len = sizeof(SIA81XX_CALIBRATION_MSG);
	head->proc_code = ch;// ch sn

	msg->msg_type = CAL_TYPE_GET_CAL;

	ret = sipa_tuning_cmd_get(
		cal_id,
		SIXTH_SIPA_RX_MODULE,
		SIXTH_SIPA_RX_CORE_PARAM,
		cmd_len,
		buf);
	if (0 > ret) {
		pr_err("[  err][%s] %s: sipa_tuning_cmd_get failed "
			"ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		return -1;
	} else {
		pr_info("[ info][%s] %s: r0(%d), t0(%d), a(%d), wire_r0(%d) : \r\n",
			LOG_FLAG, __func__,
			msg->calibrated_param.r0,
			msg->calibrated_param.t0,
			msg->calibrated_param.a,
			msg->calibrated_param.wire_r0);

		*r0 = msg->calibrated_param.r0;
		*t0 = msg->calibrated_param.t0;
		*wire_r0 = msg->calibrated_param.wire_r0;
		*a = msg->calibrated_param.a;
	}

	return 0;
}

int sipa_tuning_cmd_cal_spk_r0(
	uint32_t cal_id,
	uint32_t ch,
	int32_t t0,
	int32_t wire_r0)
{
	int ret = 0;
	uint8_t buf[MAX_TUNING_MSG_LEN];
	SIPA_TUNING_PARAM_HEAD *head = (SIPA_TUNING_PARAM_HEAD *)buf;
	SIA81XX_CALIBRATION_MSG *msg = (SIA81XX_CALIBRATION_MSG *)(head->msg);
	uint32_t cmd_len =
		sizeof(SIPA_TUNING_PARAM_HEAD) + sizeof(SIA81XX_CALIBRATION_MSG);

	memset(buf, 0, sizeof(buf));

	head->id = ID_SPEAKER_MSG;
	head->msg_len = sizeof(SIA81XX_CALIBRATION_MSG);
	head->proc_code = ch;// ch sn

	msg->msg_type = CAL_TYPE_CAL_R0;
	msg->calibration_r0.t0 = t0;
	msg->calibration_r0.wire_r0 = wire_r0;

	ret = sipa_tuning_cmd_set(
		cal_id,
		SIXTH_SIPA_RX_MODULE,
		SIXTH_SIPA_RX_CORE_PARAM,
		cmd_len,
		buf);
	if (0 > ret) {
		pr_err("[  err][%s] %s: sipa_tuning_cmd_set failed "
			"ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		return -EEXEC;
	}

	return 0;
}

int sipa_tuning_cmd_get_f0(
	uint32_t cal_id,
	uint32_t ch,
	int32_t *f0)
{
	int ret = 0;
	uint8_t buf[MAX_TUNING_MSG_LEN];
	SIPA_TUNING_PARAM_HEAD *head = (SIPA_TUNING_PARAM_HEAD *)buf;
	SIA81XX_CALIBRATION_MSG *msg = (SIA81XX_CALIBRATION_MSG *)(head->msg);
	uint32_t cmd_len =
		sizeof(SIPA_TUNING_PARAM_HEAD) + sizeof(SIA81XX_CALIBRATION_MSG);

	memset(buf, 0, sizeof(buf));

	head->id = ID_SPEAKER_MSG;
	head->msg_len = sizeof(SIA81XX_CALIBRATION_MSG);
	head->proc_code = ch;// ch sn

	msg->msg_type = CAL_TYPE_GET_F0;

	ret = sipa_tuning_cmd_get(
		cal_id,
		SIXTH_SIPA_RX_MODULE,
		SIXTH_SIPA_RX_CORE_PARAM,
		cmd_len,
		buf);
	if (0 > ret) {
		pr_err("[  err][%s] %s: cmd failed ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		return -1;
	} else {
		pr_info("[ info][%s] %s\r\n",
			LOG_FLAG, __func__);
	}

	return 0;

}

int sipa_tuning_cmd_debug_show(
	uint32_t cal_id,
	uint32_t ch)
{
	int ret = 0;
	uint8_t buf[MAX_TUNING_MSG_LEN];
	SIPA_TUNING_PARAM_HEAD *head = (SIPA_TUNING_PARAM_HEAD *)buf;
	SIA81XX_DEBUG_SHOW *msg = (SIA81XX_DEBUG_SHOW *)(head->msg);
	uint32_t cmd_len =
		sizeof(SIPA_TUNING_PARAM_HEAD) + sizeof(SIA81XX_DEBUG_SHOW);

	memset(buf, 0, sizeof(buf));

	head->id = ID_DEBUG_SH_MSG;
	head->msg_len = sizeof(SIA81XX_CALIBRATION_MSG);
	if (ch < ARRAY_SIZE(msg->proc_code)) {
		msg->proc_code[ch] = 1;
	} else {
		msg->proc_code[0] = 1;
		msg->proc_code[1] = 1;
		msg->proc_code[8] = 1;
	}

	ret = sipa_tuning_cmd_set(
		cal_id,
		SIXTH_SIPA_RX_MODULE,
		SIXTH_SIPA_RX_CORE_PARAM,
		cmd_len,
		buf);
	if (0 > ret) {
		pr_err("[  err][%s] %s: sipa_tuning_cmd_set failed "
			"ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		return -EEXEC;
	}

	return 0;
}

int sipa_tunning_cmd_set_hoc(
	uint32_t cal_id,
	uint32_t ch,
	uint32_t hoc)
{
	int ret = 0;
	uint8_t buf[MAX_TUNING_MSG_LEN];
	SIPA_TUNING_PARAM_HEAD *head = (SIPA_TUNING_PARAM_HEAD *)buf;
	AD_HOC_MSG *hoc_msg = (AD_HOC_MSG *)(head->msg);
	uint32_t cmd_len =
		sizeof(SIPA_TUNING_PARAM_HEAD) + sizeof(AD_HOC_MSG);

	memset(buf, 0, sizeof(buf));

	head->id = ID_HOC_MSG;
	head->msg_len = sizeof(AD_HOC_MSG);
	head->proc_code = ch;// ch sn

	hoc_msg->ad_hoc = hoc;

	ret = sipa_tuning_cmd_set(
		cal_id,
		SIXTH_SIPA_RX_MODULE,
		SIXTH_SIPA_RX_CORE_PARAM,
		cmd_len,
		buf);
	if (0 > ret) {
		pr_err("[  err][%s] %s: sipa_tuning_cmd_set failed ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		return -EEXEC;
	} else {
		pr_info("[ info][%s] %s", LOG_FLAG, __func__);
	}

	return 0;

}

int sipa_tuning_cmd_get_rdc_temp(
	uint32_t cal_id,
	uint32_t ch,
	int32_t *instant_f0,
	int32_t *rdc,
	int32_t *temperature
)
{
	int ret = 0;
	uint8_t buf[MAX_TUNING_MSG_LEN];
	SIPA_TUNING_PARAM_HEAD *head = (SIPA_TUNING_PARAM_HEAD *)buf;
	SIA81XX_CALIBRATION_MSG *msg = (SIA81XX_CALIBRATION_MSG *)(head->msg);
	uint32_t cmd_len =
		sizeof(SIPA_TUNING_PARAM_HEAD) + sizeof(SIA81XX_CALIBRATION_MSG);

	memset(buf, 0, sizeof(buf));

	head->id = ID_SPEAKER_MSG;
	head->msg_len = sizeof(SIA81XX_CALIBRATION_MSG);
	head->proc_code = ch;

	msg->msg_type = CAL_TYPE_GET_STATE;

	ret = sipa_tuning_cmd_get(
		cal_id,
		SIXTH_SIPA_RX_MODULE,
		SIXTH_SIPA_RX_CORE_PARAM,
		cmd_len,
		buf);
	if (0 > ret) {
		pr_err("[  err][%s] %s: cmd failed ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		return -1;
	} else {
		pr_info("[ info][%s] %s: instant_f0(%d) rdc(%d) temp(%d)\r\n",
			LOG_FLAG, __func__, msg->spk_state.instant_f0, msg->spk_state.rdc, msg->spk_state.temperature);

		*instant_f0 = msg->spk_state.instant_f0;
		*rdc = msg->spk_state.rdc;
		*temperature = msg->spk_state.temperature;
	}

	return 0;

}

static int sipa_cal_r0_param_timer_task_callback(
	int is_first,
	void *data)
{
	struct sipa_cal_r0 *calr0;
	calr0 = (struct sipa_cal_r0 *)data;

	sipa_tunning_cmd_set_hoc(
		calr0->tuning_port_id,
		calr0->ch,
		calr0->ad_hoc);

	sipa_cal_spk_execute(
		calr0->tuning_port_id,
		calr0->ch,
		SIPA_CAL_SPK_DEFAULT_VAL,
		SIPA_CAL_SPK_DEFAULT_VAL);

	return 0;
}


int sipa_auto_first_cal_r0(
	uint32_t timer_task_hdl,
	uint32_t tuning_port_id,
	uint32_t ch)
{
	int ret = 0;

	cal_r0[ch].tuning_port_id = tuning_port_id;
	cal_r0[ch].ch = ch;
	cal_r0[ch].ad_hoc = 0x3d4ccccd;

	ret = sipa_timer_task_register(
		timer_task_hdl,
		TIMER_TASK_PROC_CAL_R0,
		sipa_timer_usr_id_cal_r0(ch),
		SIPA_SPK_CAL_R0_TIME_MS,
		/* is_first > 0 means first, other means not first */
		sipa_cal_r0_param_timer_task_callback,
		&cal_r0[ch],
		1,
		0);
	if (0 > ret) {
		pr_err("[  err][%s] %s: sipa_timer_task_register ret : %d \r\n",
			LOG_FLAG, __func__, ret);
	}

	pr_info("[ info][%s] %s: done, ret(%d) \r\n",
			LOG_FLAG, __func__, ret);

	return 0;


}

int sipa_tunning_cmd_set_spk_model(
	uint32_t cal_id,
	uint32_t ch,
	int32_t f0,
	int32_t q,
	int32_t xthresh,
	int32_t xthresh_rdc)
{
	int ret = 0;
	uint8_t buf[MAX_TUNING_MSG_LEN];
	SIPA_TUNING_PARAM_HEAD *head = (SIPA_TUNING_PARAM_HEAD *)buf;
	SIA81XX_CALIBRATION_MSG *spk_model_msg = (SIA81XX_CALIBRATION_MSG *)(head->msg);
	uint32_t cmd_len =
		sizeof(SIPA_TUNING_PARAM_HEAD) + sizeof(SIA81XX_CALIBRATION_MSG);

	memset(buf, 0, sizeof(buf));

	head->id = ID_SPEAKER_MSG;
	head->msg_len = sizeof(SIA81XX_CALIBRATION_MSG);
	head->proc_code = ch;// ch sn

	spk_model_msg->msg_type = CAL_TYPE_SET_SPK_MODEL;
	spk_model_msg->spk_model_param.f0 = f0;
	spk_model_msg->spk_model_param.q = q;
	spk_model_msg->spk_model_param.xthresh = xthresh;
	spk_model_msg->spk_model_param.xthresh_rdc = xthresh_rdc;

	ret = sipa_tuning_cmd_set(
		cal_id,
		SIXTH_SIPA_RX_MODULE,
		SIXTH_SIPA_RX_CORE_PARAM,
		cmd_len,
		buf);
	if (0 > ret) {
		pr_err("[  err][%s] %s: sipa_tuning_cmd_set failed "
			"ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		return -EEXEC;
	}

	return 0;
}

static int sipa_cal_spk_model_task_callback(
	int is_first,
	void *data)
{
	struct sipa_spk_model *model;
	model = (struct sipa_spk_model *)data;

	sipa_tunning_cmd_set_spk_model(
		model->tuning_port_id,
		model->ch,
		model->spk_model.f0,
		model->spk_model.q,
		model->spk_model.xthresh,
		model->spk_model.xthresh_rdc);

	return 0;
}

int sipa_auto_set_spk_model(
	uint32_t timer_task_hdl,
	uint32_t tuning_port_id,
	uint32_t ch)
{
	int ret;
	SIPA_PARAM_SPK_MODEL_PARAM model;

	sipa_param_read_spk_model(ch, &model);
	spk_model_param[ch].tuning_port_id = tuning_port_id;
	spk_model_param[ch].ch = ch;
	spk_model_param[ch].spk_model = model;

	ret = sipa_timer_task_register(
		timer_task_hdl,
		TIMER_TASK_PROC_SET_SPK_MODEL,
		sipa_timer_usr_id_set_spk_model(ch),
		SIPA_SET_SPK_MODEL_TIME_MS,
		/* is_first > 0 means first, other means not first */
		sipa_cal_spk_model_task_callback,
		&spk_model_param[ch],
		1,
		0);

	return 0;
}

static int strcmp_spk_model_param(
	SIPA_PARAM_SPK_MODEL_PARAM *model_param,
	SIPA_PARAM_SPK_MODEL_PARAM *mode_param_original)
{
	if ((model_param->f0 == mode_param_original->f0) &&
		(model_param->q == mode_param_original->q) &&
		(model_param->xthresh == mode_param_original->xthresh) &&
		(model_param->xthresh_rdc == mode_param_original->xthresh_rdc))
		return 0;

	return 1;
}

int sipa_tunning_cmd_get_spk_model(
	uint32_t tuning_port_id,
	uint32_t ch,
	SIPA_PARAM_SPK_MODEL_PARAM *model_param)
{
	int ret = 0;
	uint8_t buf[MAX_TUNING_MSG_LEN];
	SIPA_PARAM_SPK_MODEL_PARAM mode_param_original;
	SIPA_TUNING_PARAM_HEAD *head = (SIPA_TUNING_PARAM_HEAD *)buf;
	SIA81XX_CALIBRATION_MSG *msg = (SIA81XX_CALIBRATION_MSG *)(head->msg);
	uint32_t cmd_len =
		sizeof(SIPA_TUNING_PARAM_HEAD) + sizeof(SIA81XX_CALIBRATION_MSG);

	memset(buf, 0, sizeof(buf));

	head->id = ID_SPEAKER_MSG;
	head->msg_len = sizeof(SIA81XX_CALIBRATION_MSG);
	head->proc_code = ch;// ch sn

	msg->msg_type = CAL_TYPE_GET_SPK_MODEL;

	ret = sipa_tuning_cmd_get(
		tuning_port_id,
		SIXTH_SIPA_RX_MODULE,
		SIXTH_SIPA_RX_CORE_PARAM,
		cmd_len,
		buf);
	if (0 > ret) {
		pr_err("[  err][%s] %s: sipa_tuning_cmd_get failed "
			"ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		return -1;
	} else {
		pr_info("[ info][%s] %s: f0(%d), q(%d), xthresh(%d), xthresh_rdc(%d) : \r\n",
			LOG_FLAG, __func__,
			msg->spk_model_param.f0,
			msg->spk_model_param.q,
			msg->spk_model_param.xthresh,
			msg->spk_model_param.xthresh_rdc);

		model_param->f0 = msg->spk_model_param.f0;
		model_param->q = msg->spk_model_param.q;
		model_param->xthresh = msg->spk_model_param.xthresh;
		model_param->xthresh_rdc = msg->spk_model_param.xthresh_rdc;

		sipa_param_read_spk_model(ch, &mode_param_original);

		if (strcmp_spk_model_param(model_param, &mode_param_original))
			sipa_param_write_spk_model(ch, model_param);
	}

	return 0;
}

static int sipa_cal_get_spk_model_task_callback(
	int is_first,
	void *data)
{
	struct sipa_spk_model *model;
	model = (struct sipa_spk_model *)data;

	sipa_tunning_cmd_get_spk_model(
		model->tuning_port_id,
		model->ch,
		&model->spk_model);

	return 0;
}

int sipa_auto_get_spk_model(
	uint32_t timer_task_hdl,
	uint32_t tuning_port_id,
	uint32_t ch)
{
	int ret;
	SIPA_PARAM_SPK_MODEL_PARAM model = {0};

	memset(&model, 0, sizeof(model));
	spk_model_param[ch].tuning_port_id = tuning_port_id;
	spk_model_param[ch].ch = ch;
	spk_model_param[ch].spk_model = model;

	ret = sipa_timer_task_register(
		timer_task_hdl,
		TIMER_TASK_PROC_GET_SPK_MODEL,
		sipa_timer_usr_id_get_spk_model(ch),
		SIPA_GET_SPK_MODEL_TIME_MS,
		/* is_first > 0 means first, other means not first */
		sipa_cal_get_spk_model_task_callback,
		&spk_model_param[ch],
		0,
		0);

	return 0;
}

int sipa_tuning_cmd_close_temp_limiter(
	uint32_t cal_id,
	uint32_t ch,
	uint32_t on)
{
	int ret = 0;
	uint8_t buf[MAX_TUNING_MSG_LEN];
	SIPA_TUNING_PARAM_HEAD *head = (SIPA_TUNING_PARAM_HEAD *)buf;
	TEMP_LIMITER_MSG *msg = (TEMP_LIMITER_MSG *)(head->msg);
	uint32_t cmd_len =
		sizeof(SIPA_TUNING_PARAM_HEAD) + sizeof(TEMP_LIMITER_MSG);

	memset(buf, 0, sizeof(buf));

	head->id = ID_TEMP_LIMITER;
	head->msg_len = sizeof(TEMP_LIMITER_MSG);
	head->proc_code = ch;// ch sn

	msg->on = on;
	memset(msg->arr, 0x0, sizeof(uint32_t) * 14);

	ret = sipa_tuning_cmd_set(
		cal_id,
		SIXTH_SIPA_RX_MODULE,
		SIXTH_SIPA_RX_CORE_PARAM,
		cmd_len,
		buf);
	if (0 > ret) {
		pr_err("[  err][%s] %s: sipa_tuning_cmd_set failed ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		return -EEXEC;
	} else {
		pr_info("[ info][%s] %s: on = %u : ", LOG_FLAG, __func__, on);
	}

	return 0;
}


int sipa_tuning_cmd_close_f0_tracking(
	uint32_t cal_id,
	uint32_t ch,
	uint32_t on)
{
	int ret = 0;
	uint8_t buf[MAX_TUNING_MSG_LEN];
	SIPA_TUNING_PARAM_HEAD *head = (SIPA_TUNING_PARAM_HEAD *)buf;
	F0_TRACKING_MSG *msg = (F0_TRACKING_MSG *)(head->msg);
	uint32_t cmd_len =
		sizeof(SIPA_TUNING_PARAM_HEAD) + sizeof(F0_TRACKING_MSG);

	memset(buf, 0, sizeof(buf));

	head->id = ID_F0_TRACKING;
	head->msg_len = sizeof(F0_TRACKING_MSG);
	head->proc_code = ch + 9;

	msg->on = on;
	memset(msg->arr, 0x0, sizeof(uint32_t) * 19);

	ret = sipa_tuning_cmd_set(
		cal_id,
		SIXTH_SIPA_RX_MODULE,
		SIXTH_SIPA_RX_CORE_PARAM,
		cmd_len,
		buf);
	if (0 > ret) {
		pr_err("[  err][%s] %s: sipa_tuning_cmd_set failed ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		return -EEXEC;
	} else {
		pr_info("[ info][%s] %s: on = %u : ", LOG_FLAG, __func__, on);
	}

	return 0;
}

static int sipa_close_f0_temp_task_callback(
	int is_first,
	void *data)
{
	struct sipa_f0_temp_module *param;
	param = (struct sipa_f0_temp_module *)data;

	sipa_tuning_cmd_close_temp_limiter(
		param->tuning_port_id,
		param->ch,
		0);

	sipa_tuning_cmd_close_f0_tracking(
		param->tuning_port_id,
		param->ch,
		0);

	return 0;
}

int sipa_tuning_close_temp_f0_module(
	uint32_t timer_task_hdl,
	uint32_t tuning_port_id,
	uint32_t ch)
{
	int ret;

	sipa_f0_temp[ch].ch = ch;
	sipa_f0_temp[ch].timer_task_hdl = timer_task_hdl;
	sipa_f0_temp[ch].tuning_port_id = tuning_port_id;

	ret = sipa_timer_task_register(
		timer_task_hdl,
		TIMER_TASK_PROC_CLOSE_F0_TEMP,
		sipa_timer_usr_id_close_f0_temp(ch),
		SIPA_CLOSE_F0_TEMP_TIME_MS,
		sipa_close_f0_temp_task_callback,
		&sipa_f0_temp[ch],
		1,
		0);

	return 0;
}



