// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "gpio.h"
#include "platform_common.h"

struct GPIO_PINCTRL gpio_pinctrl_list_cam[
			GPIO_CTRL_STATE_MAX_NUM_CAM] = {
	/* Main */
	{"pnd1"},
	{"pnd0"},
	{"rst1"},
	{"rst0"},
	{"ldo_vcama_1"},
	{"ldo_vcama_0"},
	{"ldo_vcama1_1"},
	{"ldo_vcama1_0"},
	{"ldo_vcamafvdd_1"},
	{"ldo_vcamafvdd_0"},
	{"ldo_vcamd_1"},
	{"ldo_vcamd_0"},
	{"ldo_vcamio_1"},
	{"ldo_vcamio_0"},
};

/* for mipi switch platform */
struct GPIO_PINCTRL gpio_pinctrl_list_switch[
			GPIO_CTRL_STATE_MAX_NUM_SWITCH] = {
	{"cam_mipi_switch_en_1"},
	{"cam_mipi_switch_en_0"},
	{"cam_mipi_switch_sel_1"},
	{"cam_mipi_switch_sel_0"}
};

static struct GPIO gpio_instance;

static enum IMGSENSOR_RETURN gpio_init(
	void *pinstance,
	struct IMGSENSOR_HW_DEVICE_COMMON *pcommon)
{
	int    i, j;
	struct GPIO            *pgpio            = (struct GPIO *)pinstance;
	enum   IMGSENSOR_RETURN ret              = IMGSENSOR_RETURN_SUCCESS;
	char str_pinctrl_name[LENGTH_FOR_SNPRINTF];
	char *lookup_names = NULL;

	pgpio->pgpio_mutex = &pcommon->pinctrl_mutex;

	pgpio->ppinctrl = devm_pinctrl_get(&pcommon->pplatform_device->dev);
	if (IS_ERR(pgpio->ppinctrl)) {
		PK_DBG("ERROR: %s, Cannot find camera pinctrl!", __func__);
		return IMGSENSOR_RETURN_ERROR;
	}

	for (j = IMGSENSOR_SENSOR_IDX_MIN_NUM;
		j < IMGSENSOR_SENSOR_IDX_MAX_NUM;
		j++) {
		for (i = 0 ; i < GPIO_CTRL_STATE_MAX_NUM_CAM; i++) {
			lookup_names =
			gpio_pinctrl_list_cam[i].ppinctrl_lookup_names;

			if (lookup_names) {
				ret = snprintf(str_pinctrl_name,
				sizeof(str_pinctrl_name),
				"cam%d_%s",
				j,
				lookup_names);
				if (ret < 0)
					PK_DBG("NOITCE: %s, snprintf err, %d\n",
						__func__,
						ret);

				pgpio->ppinctrl_state_cam[j][i] =
					pinctrl_lookup_state(
						pgpio->ppinctrl,
						str_pinctrl_name);
			}

			if (pgpio->ppinctrl_state_cam[j][i] == NULL ||
				IS_ERR(pgpio->ppinctrl_state_cam[j][i])) {
				PK_DBG("NOTICE: %s, pinctrl err, %s\n",
					__func__,
					str_pinctrl_name);
				pgpio->ppinctrl_state_cam[j][i] = NULL;
			}
		}
	}
	/* for mipi switch platform */
	for (i = 0; i < GPIO_CTRL_STATE_MAX_NUM_SWITCH; i++) {
		if (gpio_pinctrl_list_switch[i].ppinctrl_lookup_names) {
			pgpio->ppinctrl_state_switch[i] =
				pinctrl_lookup_state(
					pgpio->ppinctrl,
			gpio_pinctrl_list_switch[i].ppinctrl_lookup_names);
		}

		if (pgpio->ppinctrl_state_switch[i] == NULL ||
			IS_ERR(pgpio->ppinctrl_state_switch[i])) {
			PK_DBG("NOTICE: %s, pinctrl err, %s\n", __func__,
			gpio_pinctrl_list_switch[i].ppinctrl_lookup_names);
			pgpio->ppinctrl_state_switch[i] = NULL;
		}
	}

	return ret;
}

static enum IMGSENSOR_RETURN gpio_release(void *pinstance)
{
	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN gpio_set(
	void *pinstance,
	enum IMGSENSOR_SENSOR_IDX   sensor_idx,
	enum IMGSENSOR_HW_PIN       pin,
	enum IMGSENSOR_HW_PIN_STATE pin_state)
{
	struct pinctrl_state  *ppinctrl_state;
	struct GPIO           *pgpio = (struct GPIO *)pinstance;
	enum   GPIO_STATE      gpio_state;
	unsigned int pin_index = 0;
	unsigned int sensor_idx_uint = 0;
	/* PK_DBG("%s :debug pinctrl ENABLE, PinIdx %d, Val %d\n",
	 *	__func__, pin, pin_state);
	 */

	if (pin < IMGSENSOR_HW_PIN_PDN ||
		pin > IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL ||
		pin_state < IMGSENSOR_HW_PIN_STATE_LEVEL_0 ||
		pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH)
		return IMGSENSOR_RETURN_ERROR;

	sensor_idx_uint = sensor_idx;

	gpio_state = (pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_0)
		? GPIO_STATE_H : GPIO_STATE_L;

	pin_index = pin - IMGSENSOR_HW_PIN_PDN;

	if (pin == IMGSENSOR_HW_PIN_MIPI_SWITCH_EN)
		ppinctrl_state = pgpio->ppinctrl_state_switch[
			GPIO_CTRL_STATE_MIPI_SWITCH_EN_H + gpio_state];
	else if (pin == IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL)
		ppinctrl_state = pgpio->ppinctrl_state_switch[
			GPIO_CTRL_STATE_MIPI_SWITCH_SEL_H + gpio_state];
	else
		ppinctrl_state =
			pgpio->ppinctrl_state_cam[sensor_idx_uint][
			(pin_index << 1) + gpio_state];

	mutex_lock(pgpio->pgpio_mutex);

	if (ppinctrl_state != NULL && !IS_ERR(ppinctrl_state))
		pinctrl_select_state(pgpio->ppinctrl, ppinctrl_state);
	else
		PK_DBG("%s : pinctrl err, PinIdx %d, Val %d\n",
			__func__, pin, pin_state);

	mutex_unlock(pgpio->pgpio_mutex);

	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN gpio_dump(void *pintance)
{
#ifdef DUMP_GPIO
	PK_DBG("[sensor_dump][gpio]\n");
	gpio_dump_regs();
	PK_DBG("[sensor_dump][gpio] finish\n");
#endif
	return IMGSENSOR_RETURN_SUCCESS;
}

static struct IMGSENSOR_HW_DEVICE device = {
	.id        = IMGSENSOR_HW_ID_GPIO,
	.pinstance = (void *)&gpio_instance,
	.init      = gpio_init,
	.set       = gpio_set,
	.release   = gpio_release,
	.dump      = gpio_dump
};

enum IMGSENSOR_RETURN imgsensor_hw_gpio_open(
	struct IMGSENSOR_HW_DEVICE **pdevice)
{
	*pdevice = &device;
	return IMGSENSOR_RETURN_SUCCESS;
}

