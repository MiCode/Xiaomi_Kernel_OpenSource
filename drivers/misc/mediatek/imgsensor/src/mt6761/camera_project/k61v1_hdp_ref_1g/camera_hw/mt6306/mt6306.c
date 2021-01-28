// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "mt6306.h"

const unsigned long mt6306_pin_list[MT6306_PIN_MAX_NUM] = {
#if 0
	MT6306_GPIO_05,
	MT6306_GPIO_10,
	MT6306_GPIO_02,
	MT6306_GPIO_03,
	MT6306_GPIO_11,
	MT6306_GPIO_04,
	MT6306_GPIO_12
#endif
};


static struct MT6306 mt6306_instance;

static enum IMGSENSOR_RETURN mt6306_init(void *instance)
{
	int i = 0;

	for (i = 0; i < MT6306_PIN_MAX_NUM; i++)
		atomic_set(&mt6306_instance.enable_cnt[i], 0);
	return IMGSENSOR_RETURN_SUCCESS;
}
static enum IMGSENSOR_RETURN mt6306_release(void *instance)
{
#if 0
	int i = 0;

	for (i = 0; i < MT6306_PIN_MAX_NUM; i++) {
		atomic_set(&mt6306_instance.enable_cnt[i], 0);
		mt6306_set_gpio_out(mt6306_pin_list[i], MT6306_GPIO_OUT_LOW);
	}
#endif
	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN mt6306_set(
	void *pinstance,
	enum IMGSENSOR_SENSOR_IDX   sensor_idx,
	enum IMGSENSOR_HW_PIN       pin,
	enum IMGSENSOR_HW_PIN_STATE pin_state)
{
#if 0
	int pin_offset;
	int list_idx = 0;
	const unsigned long *ppin_list;
#endif
	enum IMGSENSOR_RETURN ret = IMGSENSOR_RETURN_SUCCESS;
#if 0
	if (pin < IMGSENSOR_HW_PIN_PDN ||
	   pin > IMGSENSOR_HW_PIN_DVDD ||
	   pin_state < IMGSENSOR_HW_PIN_STATE_LEVEL_0 ||
	   pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH)
		ret = IMGSENSOR_RETURN_ERROR;

	if (pin == IMGSENSOR_HW_PIN_AVDD ||
	  (pin == IMGSENSOR_HW_PIN_DVDD &&
	   sensor_idx == IMGSENSOR_SENSOR_IDX_MAIN2) ||
	    (pin == IMGSENSOR_HW_PIN_DVDD &&
	   sensor_idx == IMGSENSOR_SENSOR_IDX_SUB2)) {
		list_idx = MT6306_PIN_CAM_EXT_PWR_EN;

	} else {
		pin_offset =
		    (sensor_idx == IMGSENSOR_SENSOR_IDX_MAIN)
		    ? MT6306_PIN_CAM_PDN0 :
		    (sensor_idx == IMGSENSOR_SENSOR_IDX_SUB)
		    ? MT6306_PIN_CAM_PDN1 : MT6306_PIN_CAM_PDN2;

		list_idx = pin_offset + pin - IMGSENSOR_HW_PIN_PDN;
	}
	pr_debug("%s pin number %d value %d\n", __func__, list_idx, pin_state);
	ppin_list  = &mt6306_pin_list[list_idx];
	mt6306_set_gpio_dir(*ppin_list, MT6306_GPIO_DIR_OUT);

	if (pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_0) {
		if (atomic_read(&mt6306_instance.enable_cnt[list_idx]) == 0)
			mt6306_set_gpio_out(*ppin_list, MT6306_GPIO_OUT_HIGH);

		atomic_inc(&mt6306_instance.enable_cnt[list_idx]);
	} else {
		if (atomic_read(&mt6306_instance.enable_cnt[list_idx]) == 1)
			mt6306_set_gpio_out(*ppin_list, MT6306_GPIO_OUT_LOW);

		if (atomic_read(&mt6306_instance.enable_cnt[list_idx]) > 0)
			atomic_dec(&mt6306_instance.enable_cnt[list_idx]);
	}
#endif
	return ret;
}

static struct IMGSENSOR_HW_DEVICE device = {
	.pinstance = (void *)&mt6306_instance,
	.init      = mt6306_init,
	.set       = mt6306_set,
	.release   = mt6306_release,
	.id        = IMGSENSOR_HW_ID_MT6306
};

enum IMGSENSOR_RETURN imgsensor_hw_mt6306_open(
	struct IMGSENSOR_HW_DEVICE **pdevice)
{
	*pdevice = &device;
	return IMGSENSOR_RETURN_SUCCESS;
}

