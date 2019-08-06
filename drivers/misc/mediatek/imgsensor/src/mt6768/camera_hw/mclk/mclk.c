/*
 * Copyright (C) 2017 MediaTek Inc.
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

struct MCLK_PINCTRL_NAMES mclk0_pinctrl[MCLK_STATE_MAX_NUM] = {
	{"cam0_mclk_off"},
	{"cam0_mclk_2mA"},
	{"cam0_mclk_4mA"},
	{"cam0_mclk_6mA"},
	{"cam0_mclk_8mA"},
};

struct MCLK_PINCTRL_NAMES mclk1_pinctrl[MCLK_STATE_MAX_NUM] = {
	{"cam1_mclk_off"},
	{"cam1_mclk_2mA"},
	{"cam1_mclk_4mA"},
	{"cam1_mclk_6mA"},
	{"cam1_mclk_8mA"},
};

struct MCLK_PINCTRL_NAMES mclk2_pinctrl[MCLK_STATE_MAX_NUM] = {
	{"cam2_mclk_off"},
	{"cam2_mclk_2mA"},
	{"cam2_mclk_4mA"},
	{"cam2_mclk_6mA"},
	{"cam2_mclk_8mA"},
};
struct MCLK_PINCTRL_NAMES mclk3_pinctrl[MCLK_STATE_MAX_NUM] = {
	{"cam3_mclk_off"},
	{"cam3_mclk_2mA"},
	{"cam3_mclk_4mA"},
	{"cam3_mclk_6mA"},
	{"cam3_mclk_8mA"},
};
struct MCLK_PINCTRL_NAMES mclk4_pinctrl[MCLK_STATE_MAX_NUM] = {
	{"cam3_mclk_off"},
	{"cam3_mclk_2mA"},
	{"cam3_mclk_4mA"},
	{"cam3_mclk_6mA"},
	{"cam3_mclk_8mA"},
};

struct MCLK_PINCTRL_NAMES*
	mclk_pinctrl_list[IMGSENSOR_SENSOR_IDX_MAX_NUM] = {
	mclk0_pinctrl, mclk1_pinctrl, mclk2_pinctrl,
	mclk3_pinctrl, mclk4_pinctrl,
};
#define MCLK_STATE_ENABLE MCLK_STATE_ENABLE_4MA

static struct mclk mclk_instance;
static enum IMGSENSOR_RETURN mclk_release(void *pinstance)
{
	int i;
	struct mclk *pinst = (struct mclk *)pinstance;

	for (i = IMGSENSOR_SENSOR_IDX_MIN_NUM;
	    i < IMGSENSOR_SENSOR_IDX_MAX_NUM;
	    i++) {
		if (pinst->ppinctrl_state[i][MCLK_STATE_DISABLE] != NULL &&
			!IS_ERR(pinst->ppinctrl_state[i][MCLK_STATE_DISABLE])) {
			mutex_lock(&pinctrl_mutex);
			pinctrl_select_state(
			    pinst->ppinctrl,
			    pinst->ppinctrl_state[i][MCLK_STATE_DISABLE]);
			pinst->drive_current[i] = MCLK_STATE_ENABLE;
			mutex_unlock(&pinctrl_mutex);
		}

	}
	return IMGSENSOR_RETURN_SUCCESS;

}
static enum IMGSENSOR_RETURN mclk_init(void *pinstance)
{
	struct mclk *pinst = (struct mclk *)pinstance;
	struct platform_device *pplatform_dev = gpimgsensor_hw_platform_device;
	int i;
	int j;
	struct MCLK_PINCTRL_NAMES *mclk_pin_state = NULL;
	enum   IMGSENSOR_RETURN ret           = IMGSENSOR_RETURN_SUCCESS;

	pinst->ppinctrl = devm_pinctrl_get(&pplatform_dev->dev);
	if (IS_ERR(pinst->ppinctrl)) {
		pr_err("%s : Cannot find camera pinctrl!\n", __func__);
		return IMGSENSOR_RETURN_ERROR;
	}

	for (i = IMGSENSOR_SENSOR_IDX_MIN_NUM;
	    i < IMGSENSOR_SENSOR_IDX_MAX_NUM;
	    i++) {
		for (j = MCLK_STATE_DISABLE; j < MCLK_STATE_MAX_NUM; j++) {
			mclk_pin_state = mclk_pinctrl_list[i];
			mclk_pin_state += j;
			if (mclk_pin_state->ppinctrl_names) {
				pinst->ppinctrl_state[i][j] =
					pinctrl_lookup_state(pinst->ppinctrl,
						mclk_pin_state->ppinctrl_names);
				if (IS_ERR(pinst->ppinctrl_state[i][j]))
					pr_debug("%s : pinctrl err, %s\n",
						__func__,
						mclk_pin_state->ppinctrl_names);
				else {
					if (j == MCLK_STATE_DISABLE) {
						pinctrl_select_state(
							pinst->ppinctrl,
						pinst->ppinctrl_state[i][j]);
					}
				}
			}

		}

		pinst->drive_current[i] = MCLK_STATE_ENABLE;
	}

	return ret;
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

	/*pr_debug("%s : sensor_idx %d mclk_set pinctrl, PinIdx %d, Val %d\n",
	 *__func__, sensor_idx, pin, pin_state);
	 */

	if (pin_state < IMGSENSOR_HW_PIN_STATE_LEVEL_0 ||
	   pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH) {
		ret = IMGSENSOR_RETURN_ERROR;
	} else {
		state_index = (pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_0)
			? pinst->drive_current[sensor_idx]
			: MCLK_STATE_DISABLE;

		ppinctrl_state = pinst->ppinctrl_state[sensor_idx][state_index];
#if 1
		pr_debug(
			"%s : sensor_idx %d pinctrl, pin %d, pin_state %d, drive_current %d\n",
			__func__,
			sensor_idx,
			pin,
			pin_state,
			pinst->drive_current[sensor_idx]);

#endif
		mutex_lock(&pinctrl_mutex);
		if (ppinctrl_state != NULL && !IS_ERR(ppinctrl_state))
			pinctrl_select_state(pinst->ppinctrl, ppinctrl_state);
		else
			pr_err(
			    "%s : sensor_idx %d fail to set pinctrl, PinIdx %d, Val %d drive current %d\n",
			    __func__,
			    sensor_idx,
			    pin,
			    pin_state,
				pinst->drive_current[sensor_idx]);
		mutex_unlock(&pinctrl_mutex);
	}
	return ret;
}

static struct IMGSENSOR_HW_DEVICE device = {
	.pinstance = (void *)&mclk_instance,
	.init      = mclk_init,
	.set       = mclk_set,
	.release   = mclk_release,
	.id        = IMGSENSOR_HW_ID_MCLK
};

enum IMGSENSOR_RETURN imgsensor_hw_mclk_open(
	struct IMGSENSOR_HW_DEVICE **pdevice)
{
	*pdevice = &device;
	gimgsensor.mclk_set_drive_current = __mclk_set_drive_current;
	return IMGSENSOR_RETURN_SUCCESS;
}

