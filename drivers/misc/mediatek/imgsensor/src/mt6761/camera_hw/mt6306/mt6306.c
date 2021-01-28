// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "mt6306.h"

const unsigned long mt6306_pin_list[MT6306_PIN_MAX_NUM] = {

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
	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN mt6306_set(
	void *pinstance,
	enum IMGSENSOR_SENSOR_IDX   sensor_idx,
	enum IMGSENSOR_HW_PIN       pin,
	enum IMGSENSOR_HW_PIN_STATE pin_state)
{
	enum IMGSENSOR_RETURN ret = IMGSENSOR_RETURN_SUCCESS;
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

