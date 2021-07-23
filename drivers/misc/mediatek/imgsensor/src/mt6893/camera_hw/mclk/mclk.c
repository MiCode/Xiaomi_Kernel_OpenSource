/*
 * Copyright (C) 2017 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "mclk.h"
struct MCLK_PINCTRL_NAMES mclk_pinctrl[MCLK_STATE_MAX_NUM] = {
	{"off"},
	{"2mA"},
	{"4mA"},
	{"6mA"},
	{"8mA"},
};
#define MCLK_STATE_ENABLE MCLK_STATE_ENABLE_4MA

static struct mclk mclk_instance;

static enum IMGSENSOR_RETURN mclk_init(
	void *pinstance,
	struct IMGSENSOR_HW_DEVICE_COMMON *pcommon)
{
	struct mclk            *pinst         = (struct mclk *)pinstance;
	enum   IMGSENSOR_RETURN ret           = IMGSENSOR_RETURN_SUCCESS;
	int i;
	int j;
	char str_pinctrl_name[LENGTH_FOR_SNPRINTF];

	pinst->pmclk_mutex = &pcommon->pinctrl_mutex;

	pinst->ppinctrl = devm_pinctrl_get(&pcommon->pplatform_device->dev);
	if (IS_ERR(pinst->ppinctrl)) {
		PK_PR_ERR("%s : Cannot find camera pinctrl!\n", __func__);
		/* ret = IMGSENSOR_RETURN_ERROR; */
		return IMGSENSOR_RETURN_ERROR;
	}

	for (i = IMGSENSOR_SENSOR_IDX_MIN_NUM;
		i < IMGSENSOR_SENSOR_IDX_MAX_NUM;
		i++) {
		for (j = MCLK_STATE_DISABLE; j < MCLK_STATE_MAX_NUM; j++) {
			if (mclk_pinctrl[j].ppinctrl_names) {
				snprintf(str_pinctrl_name,
					sizeof(str_pinctrl_name),
					"cam%d_mclk_%s",
					i,
					mclk_pinctrl[j].ppinctrl_names);
				pinst->ppinctrl_state[i][j] =
					pinctrl_lookup_state(pinst->ppinctrl,
						str_pinctrl_name);

				mutex_lock(pinst->pmclk_mutex);
				if (IS_ERR(pinst->ppinctrl_state[i][j]))
					pr_debug("%s : pinctrl err, %s\n",
						__func__,
						str_pinctrl_name);
				else {
					if (j == MCLK_STATE_DISABLE) {
						pinctrl_select_state(
							pinst->ppinctrl,
						pinst->ppinctrl_state[i][j]);
					}
				}
				mutex_unlock(pinst->pmclk_mutex);

			}

		}
		pinst->drive_current[i] = MCLK_STATE_ENABLE;
	}

	return ret;
}

static enum IMGSENSOR_RETURN mclk_release(void *pinstance)
{
	int i;
	struct mclk *pinst = (struct mclk *)pinstance;

	for (i = IMGSENSOR_SENSOR_IDX_MIN_NUM;
			i < IMGSENSOR_SENSOR_IDX_MAX_NUM;
			i++) {
		mutex_lock(pinst->pmclk_mutex);
		if (pinst->ppinctrl_state[i][MCLK_STATE_DISABLE] != NULL &&
			!IS_ERR(pinst->ppinctrl_state[i][MCLK_STATE_DISABLE]))
			pinctrl_select_state(pinst->ppinctrl,
				pinst->ppinctrl_state[i][MCLK_STATE_DISABLE]);
		pinst->drive_current[i] = MCLK_STATE_ENABLE;
		mutex_unlock(pinst->pmclk_mutex);
	}
	return IMGSENSOR_RETURN_SUCCESS;
}

#define _TO_MCLK_STATE(x) (x+1)
static enum IMGSENSOR_RETURN __mclk_set_drive_current(
	void *pinstance,
	enum IMGSENSOR_SENSOR_IDX sensor_idx,
	enum ISP_DRIVING_CURRENT_ENUM target_current)
{
	struct mclk *pinst = (struct mclk *)pinstance;

	/*pr_debug("%s : sensor_idx %d, drive_current %d\n",
	 *	__func__,
	 *	sensor_idx,
	 *	target_current);
	 */
	if (_TO_MCLK_STATE(target_current) < MCLK_STATE_ENABLE_2MA ||
		_TO_MCLK_STATE(target_current) > MCLK_STATE_ENABLE_8MA) {
		pr_debug("%s : sensor_idx %d, drive_current %d, set as 4mA\n",
			__func__,
			sensor_idx,
			_TO_MCLK_STATE(target_current));
		pinst->drive_current[sensor_idx] = MCLK_STATE_ENABLE_4MA;
	} else
		pinst->drive_current[sensor_idx] =
				_TO_MCLK_STATE(target_current);
	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN mclk_set(
	void *pinstance,
	enum IMGSENSOR_SENSOR_IDX   sensor_idx,
	enum IMGSENSOR_HW_PIN       pin,
	enum IMGSENSOR_HW_PIN_STATE pin_state)
{
	struct mclk *pinst = (struct mclk *)pinstance;
	struct pinctrl_state *ppinctrl_state;
	enum   IMGSENSOR_RETURN ret = IMGSENSOR_RETURN_SUCCESS;
	enum MCLK_STATE state_index = MCLK_STATE_DISABLE;


	if (pin_state < IMGSENSOR_HW_PIN_STATE_LEVEL_0 ||
	    pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH) {
		ret = IMGSENSOR_RETURN_ERROR;
	} else {
		state_index = (pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_0)
		? pinst->drive_current[sensor_idx]
		: MCLK_STATE_DISABLE;

		ppinctrl_state =
			pinst->ppinctrl_state[sensor_idx][state_index];
#if 0
		pr_debug(
			"%s : sensor_idx %d pinctrl, pin %d, pin_state %d, drive_current %d\n",
			__func__,
			sensor_idx,
			pin,
			pin_state,
			pinst->drive_current[sensor_idx]);

#endif
		mutex_lock(pinst->pmclk_mutex);

		if (!IS_ERR(ppinctrl_state))
			pinctrl_select_state(pinst->ppinctrl, ppinctrl_state);
		else
			PK_PR_ERR(
				"%s : sensor_idx %d pinctrl, PinIdx %d, Val %d, drive current %d\n",
				__func__,
				sensor_idx,
				pin,
				pin_state,
				pinst->drive_current[sensor_idx]);

		mutex_unlock(pinst->pmclk_mutex);
	}
	return ret;
}

static struct IMGSENSOR_HW_DEVICE device = {
	.id        = IMGSENSOR_HW_ID_MCLK,
	.pinstance = (void *)&mclk_instance,
	.init      = mclk_init,
	.set       = mclk_set,
	.release   = mclk_release,
};

enum IMGSENSOR_RETURN imgsensor_hw_mclk_open(
	struct IMGSENSOR_HW_DEVICE **pdevice)
{
	*pdevice = &device;
	gimgsensor.mclk_set_drive_current = __mclk_set_drive_current;
	return IMGSENSOR_RETURN_SUCCESS;
}

