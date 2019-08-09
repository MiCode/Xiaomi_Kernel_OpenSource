/*
 * Copyright (C) 2017 MediaTek Inc.
 * Copyright (C) 2019 XiaoMi, Inc.
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

#include "gpio.h"

struct GPIO_PINCTRL gpio_pinctrl_list[GPIO_CTRL_STATE_MAX_NUM] = {
	/* Main */
	{"cam0_pnd1"},
	{"cam0_pnd0"},
	{"cam0_rst1"},
	{"cam0_rst0"},
	{NULL},
	{NULL},
	{"cam0_vcamd_on"},
	{"cam0_vcamd_off"},
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	/* Sub */
	{"cam1_pnd1"},
	{"cam1_pnd0"},
	{"cam1_rst1"},
	{"cam1_rst0"},
	{"cam1_vcama_on"},
	{"cam1_vcama_off"},
	{"cam1_vcamd_on"},
	{"cam1_vcamd_off"},
	/* Main2 */
	{"cam2_pnd1"},
	{"cam2_pnd0"},
	{"cam2_rst1"},
	{"cam2_rst0"},
	{"cam2_vcama_on"},
	{"cam2_vcama_off"},
	{"cam2_vcamd_on"},
	{"cam2_vcamd_off"},
	/* Sub2 */
	{"cam3_pnd1"},
	{"cam3_pnd0"},
	{"cam3_rst1"},
	{"cam3_rst0"},
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	/* Main3 */
	{"cam4_pnd1"},
	{"cam4_pnd0"},
	{"cam4_rst1"},
	{"cam4_rst0"},
	{"cam4_vcama_on"},
	{"cam4_vcama_off"},
	{"cam4_vcamd_on"},
	{"cam4_vcamd_off"},

#ifdef MIPI_SWITCH
	{"cam_mipi_switch_en_1"},
	{"cam_mipi_switch_en_0"},
	{"cam_mipi_switch_sel_1"},
	{"cam_mipi_switch_sel_0"}
#endif
};

static struct GPIO gpio_instance;

/*
 * reset all state of gpio to default value
 */
static enum IMGSENSOR_RETURN gpio_release(void *pinstance)
{
	int    i;
	struct platform_device *pplatform_dev = gpimgsensor_hw_platform_device;
	struct GPIO            *pgpio         = (struct GPIO *)pinstance;
	enum   IMGSENSOR_RETURN ret           = IMGSENSOR_RETURN_SUCCESS;

	pgpio->ppinctrl = devm_pinctrl_get(&pplatform_dev->dev);
	if (IS_ERR(pgpio->ppinctrl))
		return IMGSENSOR_RETURN_ERROR;

	for (i = GPIO_CTRL_STATE_CAM0_PDN_L;
		i < GPIO_CTRL_STATE_MAX_NUM;
		i += 2) {
		mutex_lock(&pinctrl_mutex);
		if (gpio_pinctrl_list[i].ppinctrl_lookup_names != NULL &&
			pgpio->ppinctrl_state[i] != NULL &&
			  !IS_ERR(pgpio->ppinctrl_state[i]) &&
				pinctrl_select_state(pgpio->ppinctrl,
						pgpio->ppinctrl_state[i])) {
			pr_info(
			    "%s : pinctrl err, PinIdx %d name %s\n",
			    __func__,
			    i,
			    gpio_pinctrl_list[i].ppinctrl_lookup_names);
		}
		mutex_unlock(&pinctrl_mutex);
	}

	return ret;
}
static enum IMGSENSOR_RETURN gpio_init(void *pinstance)
{
	int    i;
	struct platform_device *pplatform_dev = gpimgsensor_hw_platform_device;
	struct GPIO            *pgpio         = (struct GPIO *)pinstance;
	struct GPIO_PINCTRL    *pgpio_pinctrl = gpio_pinctrl_list;
	enum   IMGSENSOR_RETURN ret           = IMGSENSOR_RETURN_SUCCESS;

	pgpio->ppinctrl = devm_pinctrl_get(&pplatform_dev->dev);
	if (IS_ERR(pgpio->ppinctrl)) {
		pr_err("%s : Cannot find camera pinctrl!", __func__);
		return IMGSENSOR_RETURN_ERROR;
	}

	for (i = 0; i < GPIO_CTRL_STATE_MAX_NUM; i++, pgpio_pinctrl++) {
		if (pgpio_pinctrl->ppinctrl_lookup_names) {
			pgpio->ppinctrl_state[i] =
				pinctrl_lookup_state(
				    pgpio->ppinctrl,
				    pgpio_pinctrl->ppinctrl_lookup_names);

			if (pgpio->ppinctrl_state[i] == NULL ||
			    IS_ERR(pgpio->ppinctrl_state[i])) {
				pr_info(
				    "%s : pinctrl err, %s\n",
				    __func__,
				    pgpio_pinctrl->ppinctrl_lookup_names);

				ret = IMGSENSOR_RETURN_ERROR;
			}
		}
	}
	return ret;
}

static enum IMGSENSOR_RETURN gpio_set(
	void *pinstance,
	enum IMGSENSOR_SENSOR_IDX   sensor_idx,
	enum IMGSENSOR_HW_PIN       pin,
	enum IMGSENSOR_HW_PIN_STATE pin_state)
{
	struct pinctrl_state        *ppinctrl_state;
	struct GPIO                 *pgpio = (struct GPIO *)pinstance;
	enum   GPIO_STATE            gpio_state;
	enum   GPIO_CTRL_STATE       ctrl_state_offset;

	if (pin < IMGSENSOR_HW_PIN_PDN ||
#ifdef MIPI_SWITCH
	   pin > IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL ||
#else
	   pin > IMGSENSOR_HW_PIN_AFVDD ||
#endif
	   pin_state < IMGSENSOR_HW_PIN_STATE_LEVEL_0 ||
	   pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH)
		return IMGSENSOR_RETURN_ERROR;

	gpio_state = (pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_0)
	    ? GPIO_STATE_H : GPIO_STATE_L;

#ifdef MIPI_SWITCH
	if (pin == IMGSENSOR_HW_PIN_MIPI_SWITCH_EN)
		ppinctrl_state = pgpio->ppinctrl_state[
		    GPIO_CTRL_STATE_MIPI_SWITCH_EN_H + gpio_state];

	else if (pin == IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL)
		ppinctrl_state = pgpio->ppinctrl_state[
		    GPIO_CTRL_STATE_MIPI_SWITCH_SEL_H + gpio_state];

	else
#endif
	{
		ctrl_state_offset =
		(sensor_idx == IMGSENSOR_SENSOR_IDX_MAIN)
			? GPIO_CTRL_STATE_CAM0_PDN_H :

		(sensor_idx == IMGSENSOR_SENSOR_IDX_SUB)
			? GPIO_CTRL_STATE_CAM1_PDN_H :

		(sensor_idx == IMGSENSOR_SENSOR_IDX_MAIN2)
			? GPIO_CTRL_STATE_CAM2_PDN_H :

		(sensor_idx == IMGSENSOR_SENSOR_IDX_SUB2)
			? GPIO_CTRL_STATE_CAM3_PDN_H :
			GPIO_CTRL_STATE_CAM4_PDN_H;

		ppinctrl_state =
		    pgpio->ppinctrl_state[ctrl_state_offset +
			((pin - IMGSENSOR_HW_PIN_PDN) << 1) + gpio_state];
	}
	/*pr_debug("%s : pinctrl , state indx %d\n",
	 *	    __func__,
	 *	    ctrl_state_offset +
	 *	    ((pin - IMGSENSOR_HW_PIN_PDN) << 1) + gpio_state);
	 */

	mutex_lock(&pinctrl_mutex);
	if (ppinctrl_state == NULL ||
		IS_ERR(ppinctrl_state) ||
		pinctrl_select_state(pgpio->ppinctrl, ppinctrl_state))
		pr_err(
		    "%s : pinctrl err, PinIdx %d, Val %d\n",
		    __func__,
		    pin, pin_state);

	mutex_unlock(&pinctrl_mutex);

	return IMGSENSOR_RETURN_SUCCESS;
}

static struct IMGSENSOR_HW_DEVICE device = {
	.pinstance = (void *)&gpio_instance,
	.init      = gpio_init,
	.set       = gpio_set,
	.release   = gpio_release,
	.id        = IMGSENSOR_HW_ID_GPIO
};

enum IMGSENSOR_RETURN imgsensor_hw_gpio_open(
	struct IMGSENSOR_HW_DEVICE **pdevice)
{
	*pdevice = &device;
	return IMGSENSOR_RETURN_SUCCESS;
}

