// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include "cam_debug_util.h"
#include "cam_res_mgr_api.h"
#include "cam_res_mgr_private.h"
#include "camera_main.h"

static struct cam_res_mgr *cam_res;

static void cam_res_mgr_free_res(void)
{
	struct cam_dev_res *dev_res, *dev_temp;
	struct cam_gpio_res *gpio_res, *gpio_temp;
	struct cam_flash_res *flash_res, *flash_temp;

	if (!cam_res)
		return;

	mutex_lock(&cam_res->gpio_res_lock);
	list_for_each_entry_safe(gpio_res, gpio_temp,
		&cam_res->gpio_res_list, list) {
		list_for_each_entry_safe(dev_res, dev_temp,
			&gpio_res->dev_list, list) {
			list_del_init(&dev_res->list);
			kfree(dev_res);
		}
		list_del_init(&gpio_res->list);
		kfree(gpio_res);
	}
	mutex_unlock(&cam_res->gpio_res_lock);

	mutex_lock(&cam_res->flash_res_lock);
	list_for_each_entry_safe(flash_res, flash_temp,
		&cam_res->flash_res_list, list) {
		list_del_init(&flash_res->list);
		kfree(flash_res);
	}
	mutex_unlock(&cam_res->flash_res_lock);
}

void cam_res_mgr_led_trigger_register(const char *name, struct led_trigger **tp)
{
	bool found = false;
	struct cam_flash_res *flash_res;

	if (!cam_res) {
		/*
		 * If this driver not probed, then just register the
		 * led trigger.
		 */
		led_trigger_register_simple(name, tp);
		return;
	}

	mutex_lock(&cam_res->flash_res_lock);
	list_for_each_entry(flash_res, &cam_res->flash_res_list, list) {
		if (!strcmp(flash_res->name, name)) {
			found = true;
			break;
		}
	}
	mutex_unlock(&cam_res->flash_res_lock);

	if (found) {
		*tp = flash_res->trigger;
	} else {
		flash_res = kzalloc(sizeof(struct cam_flash_res), GFP_KERNEL);
		if (!flash_res) {
			CAM_ERR(CAM_RES,
				"Failed to malloc memory for flash_res:%s",
				name);
			*tp = NULL;
			return;
		}

		led_trigger_register_simple(name, tp);
		INIT_LIST_HEAD(&flash_res->list);
		flash_res->trigger = *tp;
		flash_res->name = name;

		mutex_lock(&cam_res->flash_res_lock);
		list_add_tail(&flash_res->list, &cam_res->flash_res_list);
		mutex_unlock(&cam_res->flash_res_lock);
	}
}
EXPORT_SYMBOL(cam_res_mgr_led_trigger_register);

void cam_res_mgr_led_trigger_unregister(struct led_trigger *tp)
{
	bool found = false;
	struct cam_flash_res *flash_res;

	if (!cam_res) {
		/*
		 * If this driver not probed, then just unregister the
		 * led trigger.
		 */
		led_trigger_unregister_simple(tp);
		return;
	}

	mutex_lock(&cam_res->flash_res_lock);
	list_for_each_entry(flash_res, &cam_res->flash_res_list, list) {
		if (flash_res->trigger == tp) {
			found = true;
			break;
		}
	}

	if (found) {
		led_trigger_unregister_simple(tp);
		list_del_init(&flash_res->list);
		kfree(flash_res);
	}
	mutex_unlock(&cam_res->flash_res_lock);
}
EXPORT_SYMBOL(cam_res_mgr_led_trigger_unregister);

void cam_res_mgr_led_trigger_event(struct led_trigger *trig,
	enum led_brightness brightness)
{
	bool found = false;
	struct cam_flash_res *flash_res;

	if (!cam_res) {
		/*
		 * If this driver not probed, then just trigger
		 * the led event.
		 */
		led_trigger_event(trig, brightness);
		return;
	}

	mutex_lock(&cam_res->flash_res_lock);
	list_for_each_entry(flash_res, &cam_res->flash_res_list, list) {
		if (flash_res->trigger == trig) {
			found = true;
			break;
		}
	}
	mutex_unlock(&cam_res->flash_res_lock);

	if (found)
		led_trigger_event(trig, brightness);
}
EXPORT_SYMBOL(cam_res_mgr_led_trigger_event);

int cam_res_mgr_util_get_idx_from_shared_pctrl_gpio(
	uint gpio)
{
	int index = 0;
	struct cam_res_mgr_dt *dt = &cam_res->dt;

	for (index = 0; index < dt->num_shared_pctrl_gpio; index++) {
		if (gpio == dt->shared_pctrl_gpio[index])
			break;
	}

	if (index == dt->num_shared_pctrl_gpio)
		return -EINVAL;

	return index;
}
EXPORT_SYMBOL(cam_res_mgr_util_get_idx_from_shared_pctrl_gpio);

int cam_res_mgr_util_get_idx_from_shared_gpio(
	uint gpio)
{
	int index = 0;
	struct cam_res_mgr_dt *dt = &cam_res->dt;

	for (index = 0; index < dt->num_shared_gpio; index++) {
		if (gpio == dt->shared_gpio[index])
			break;
	}

	if (index == dt->num_shared_gpio)
		return -EINVAL;

	return index;
}
EXPORT_SYMBOL(cam_res_mgr_util_get_idx_from_shared_gpio);

static bool cam_res_mgr_gpio_is_in_shared_pctrl_gpio(
	uint gpio)
{
	int index = 0;
	bool found = false;
	struct cam_res_mgr_dt *dt = &cam_res->dt;

	for (index = 0; index < dt->num_shared_pctrl_gpio; index++) {
		if (gpio == dt->shared_pctrl_gpio[index]) {
			found = true;
			break;
		}
	}

	return found;
}

static bool cam_res_mgr_gpio_is_in_shared_gpio(
	uint gpio)
{
	int index = 0;
	bool found = false;
	struct cam_res_mgr_dt *dt = &cam_res->dt;

	for (; index < dt->num_shared_gpio; index++) {
		if (gpio == dt->shared_gpio[index]) {
			found = true;
			break;
		}
	}

	return found;
}

int cam_res_mgr_util_shared_gpio_check_hold(uint gpio)
{
	int index = 0;
	int dev_num = 0;
	struct list_head *list;
	struct cam_gpio_res *gpio_res = NULL;
	struct cam_res_mgr_dt *dt =  NULL;
	bool is_shared_gpio = false;
	bool is_shared_pctrl_gpio = false;

	if (!cam_res) {
		CAM_DBG(CAM_RES, "res mgr is not initilized");
		return 0;
	}

	if (!cam_res->shared_gpio_enabled) {
		CAM_DBG(CAM_RES, "Res_mgr is not sharing any gpios");
		return 0;
	}

	dt = &cam_res->dt;

	is_shared_gpio =
		cam_res_mgr_gpio_is_in_shared_gpio(gpio);
	is_shared_pctrl_gpio =
		cam_res_mgr_gpio_is_in_shared_pctrl_gpio(gpio);

	if (is_shared_gpio && is_shared_pctrl_gpio) {
		CAM_ERR(CAM_RES,
			"gpio %u cannot be shared between pinctrl and gpio");
		return -EINVAL;
	}

	if (is_shared_gpio) {
		index = cam_res_mgr_util_get_idx_from_shared_gpio(gpio);
		if (index < 0) {
			CAM_ERR(CAM_RES, "Gpio%u not found in shared gpio list",
				gpio);
			return -EINVAL;
		}

		list_for_each_entry(gpio_res,
			&cam_res->gpio_res_list, list) {

			if (gpio_res->gpio ==
				dt->shared_gpio[index]) {
				list_for_each(list, &gpio_res->dev_list)
					dev_num++;

				if (dev_num >= 2) {
					return RES_MGR_GPIO_NEED_HOLD;
				}
			}
		}
	}

	if (is_shared_pctrl_gpio) {
		index = cam_res_mgr_util_get_idx_from_shared_pctrl_gpio(gpio);
		if (index < 0) {
			CAM_ERR(CAM_RES,
				"gpio%u not found in shared pctrl gpio list",
				gpio);
			return -EINVAL;
		}

		list_for_each_entry(gpio_res,
			&cam_res->gpio_res_list, list) {
			if (gpio_res->gpio ==
				dt->shared_pctrl_gpio[index]) {
				list_for_each(list, &gpio_res->dev_list)
					dev_num++;

				if (dev_num >= 2) {
					CAM_DBG(CAM_RES,
						"gpio: %u needs to hold", gpio);
					return RES_MGR_GPIO_NEED_HOLD;
				}
			}
		}
	}

	for (index = 0; index < dt->num_shared_gpio; index++) {
		list_for_each_entry(gpio_res,
			&cam_res->gpio_res_list, list) {

			if (gpio_res->gpio ==
				dt->shared_gpio[index]) {
				list_for_each(list, &gpio_res->dev_list)
					dev_num++;

				if (dev_num >= 2) {
					CAM_DBG(CAM_RES,
						"gpio: %u needs to hold", gpio);
					return RES_MGR_GPIO_NEED_HOLD;
				}
			}
		}
	}

	for (index = 0; index < dt->num_shared_pctrl_gpio; index++) {
		list_for_each_entry(gpio_res,
			&cam_res->gpio_res_list, list) {

			if (gpio_res->gpio ==
				dt->shared_pctrl_gpio[index]) {
				list_for_each(list, &gpio_res->dev_list)
					dev_num++;

				if (dev_num >= 2) {
					CAM_DBG(CAM_RES,
						"gpio: %u needs to hold", gpio);
					return RES_MGR_GPIO_NEED_HOLD;
				}
			}
		}
	}

	CAM_DBG(CAM_RES, "gpio: %u can free the resource", gpio);
	return RES_MGR_GPIO_CAN_FREE;
}
EXPORT_SYMBOL(cam_res_mgr_util_shared_gpio_check_hold);

static int cam_res_mgr_shared_pinctrl_select_state(
	int idx, bool active)
{
	int rc = 0;

	if (!cam_res || !cam_res->shared_gpio_enabled) {
		CAM_DBG(CAM_RES, "Not support shared gpio.");
		return 0;
	}

	if (cam_res->pctrl_res[idx].pstatus == PINCTRL_STATUS_PUT) {
		CAM_DBG(CAM_RES, "The shared pinctrl alerady been put.!");
		return 0;
	}

	if (active &&
		(cam_res->pctrl_res[idx].pstatus != PINCTRL_STATUS_ACTIVE)) {
		CAM_DBG(CAM_RES,
			"pinctrl select state to active for the shared_pctrl_gpio idx: %d",
			idx);
		rc = pinctrl_select_state(cam_res->pinctrl,
			cam_res->pctrl_res[idx].active);
		cam_res->pctrl_res[idx].pstatus = PINCTRL_STATUS_ACTIVE;
	} else if (!active &&
		(cam_res->pctrl_res[idx].pstatus == PINCTRL_STATUS_ACTIVE)) {
		CAM_DBG(CAM_RES,
			"pinctrl select state to suspend for the shared_pctrl_gpio idx: %d",
			idx);
		rc = pinctrl_select_state(cam_res->pinctrl,
			cam_res->pctrl_res[idx].suspend);
		cam_res->pctrl_res[idx].pstatus = PINCTRL_STATUS_SUSPEND;
	}

	return 0;
}

static int cam_res_mgr_add_device(struct device *dev,
	struct cam_gpio_res *gpio_res)
{
	struct cam_dev_res *dev_res = NULL;

	dev_res = kzalloc(sizeof(struct cam_dev_res), GFP_KERNEL);
	if (!dev_res)
		return -ENOMEM;

	dev_res->dev = dev;
	INIT_LIST_HEAD(&dev_res->list);
	list_add_tail(&dev_res->list, &gpio_res->dev_list);

	return 0;
}

static struct cam_gpio_res *cam_res_mgr_find_if_gpio_in_list(uint gpio)
{
	struct cam_gpio_res *gpio_res = NULL;

	list_for_each_entry(gpio_res, &cam_res->gpio_res_list, list) {
		if (gpio == gpio_res->gpio)
			return gpio_res;
	}

	return NULL;
}

static bool __cam_res_mgr_find_if_gpio_is_shared(uint gpio)
{
	bool found_in_shared_gpio = false;
	bool found_in_shared_pctrl_gpio = false;

	found_in_shared_gpio = cam_res_mgr_gpio_is_in_shared_gpio(gpio);

	found_in_shared_pctrl_gpio =
		cam_res_mgr_gpio_is_in_shared_pctrl_gpio(gpio);

	if (found_in_shared_pctrl_gpio && found_in_shared_gpio) {
		CAM_WARN(CAM_RES, "gpio: %u cannot be shared in both list",
			gpio);
		return false;
	}

	if (found_in_shared_pctrl_gpio || found_in_shared_gpio)
		return true;

	return false;
}


int cam_res_mgr_gpio_request(struct device *dev, uint gpio,
		unsigned long flags, const char *label)
{
	int                          rc = 0;
	bool                         dev_found = false;
	bool                         gpio_found = false;
	int                          pctrl_idx = -1;
	struct cam_gpio_res         *gpio_res = NULL;

	mutex_lock(&cam_res->gpio_res_lock);
	if (cam_res && cam_res->shared_gpio_enabled) {
		gpio_res = cam_res_mgr_find_if_gpio_in_list(gpio);
		if (gpio_res == NULL)
			gpio_found = false;
		else
			gpio_found = true;
	}

	/*
	 * gpio_found equal to false has two situation:
	 * 1. shared gpio/pinctrl_gpio not enabled
	 * 2. shared gpio/pinctrl_gpio enabled, but not find this gpio
	 *    from the gpio_res_list
	 * These two situations both need request gpio.
	 */
	if (!gpio_found) {
		CAM_DBG(CAM_RES, "gpio: %u not found in gpio_res list", gpio);
		rc = gpio_request_one(gpio, flags, label);
		if (rc) {
			CAM_ERR(CAM_RES, "gpio %d:%s request fails rc = %d",
				gpio, label, rc);
			goto end;
		}
	}

	/*
	 * If the gpio is in the shared list, and not find
	 * from gpio_res_list, then insert a cam_gpio_res
	 * to gpio_res_list.
	 */
	if ((!gpio_found && cam_res
		&& cam_res->shared_gpio_enabled) &&
		(cam_res_mgr_gpio_is_in_shared_gpio(gpio) ||
		(cam_res_mgr_gpio_is_in_shared_pctrl_gpio(gpio)))) {
		CAM_DBG(CAM_RES, "gpio: %u is shared", gpio);

		gpio_res = kzalloc(sizeof(struct cam_gpio_res), GFP_KERNEL);
		if (!gpio_res) {
			rc = -ENOMEM;
			goto end;
		}
		gpio_res->gpio = gpio;
		gpio_res->power_on_count = 0;
		INIT_LIST_HEAD(&gpio_res->list);
		INIT_LIST_HEAD(&gpio_res->dev_list);

		rc = cam_res_mgr_add_device(dev, gpio_res);
		if (rc) {
			kfree(gpio_res);
			goto end;
		}

		list_add_tail(&gpio_res->list, &cam_res->gpio_res_list);
	}

	/* if shared gpio is in pinctrl gpio list */
	if (!gpio_found && cam_res
		&& cam_res->shared_gpio_enabled &&
		cam_res_mgr_gpio_is_in_shared_pctrl_gpio(gpio)) {
		pctrl_idx =
			cam_res_mgr_util_get_idx_from_shared_pctrl_gpio(gpio);
		CAM_DBG(CAM_RES,
			"shared_pctrl_gpio is at idx: %d", pctrl_idx);
		if (pctrl_idx < 0) {
			CAM_ERR(CAM_RES,
				"pctrl_gpio: %u not found", gpio);
			rc = -EINVAL;
			goto end;
		}

		/* Update Pinctrl state to active */
		cam_res_mgr_shared_pinctrl_select_state(pctrl_idx, true);
	}

	if (gpio_found && cam_res
		&& cam_res->shared_gpio_enabled) {
		struct cam_dev_res *dev_res = NULL;

		list_for_each_entry(dev_res, &gpio_res->dev_list, list) {
			if (dev_res->dev == dev) {
				dev_found = true;
				break;
			}
		}

		if (!dev_found) {
			rc = cam_res_mgr_add_device(dev, gpio_res);
			if (rc) {
				CAM_ERR(CAM_RES,
					"add device to gpio res list failed rc: %d",
					rc);
				goto end;
			}
		}
	}

end:
	mutex_unlock(&cam_res->gpio_res_lock);
	return rc;
}
EXPORT_SYMBOL(cam_res_mgr_gpio_request);

bool cam_res_mgr_util_check_if_gpio_is_shared(
	struct gpio *gpio_tbl, uint8_t size)
{
	int i = 0;
	bool found = false;

	if (!cam_res) {
		CAM_DBG(CAM_RES, "cam_res data is not avaialbe");
		return false;
	}

	if (!cam_res->shared_gpio_enabled) {
		CAM_DBG(CAM_RES, "shared gpio support is not enabled");
		return false;
	}

	for (i = 0; i < size; i++) {
		found = __cam_res_mgr_find_if_gpio_is_shared(gpio_tbl[i].gpio);
		if (found)
			return found;
	}

	return false;
}
EXPORT_SYMBOL(cam_res_mgr_util_check_if_gpio_is_shared);

static void cam_res_mgr_gpio_free(struct device *dev, uint gpio)
{
	bool                   gpio_found = false;
	bool                   need_free = true;
	int                    dev_num = 0;
	struct cam_gpio_res   *gpio_res = NULL;
	bool                   is_shared_gpio = false;
	bool                   is_shared_pctrl_gpio = false;
	int                    pctrl_idx = -1;

	is_shared_gpio = cam_res_mgr_gpio_is_in_shared_gpio(gpio);
	is_shared_pctrl_gpio =
			cam_res_mgr_gpio_is_in_shared_pctrl_gpio(gpio);

	mutex_lock(&cam_res->gpio_res_lock);
	if (cam_res && cam_res->shared_gpio_enabled) {
		list_for_each_entry(gpio_res, &cam_res->gpio_res_list, list) {
			if (gpio == gpio_res->gpio) {
				gpio_found = true;
				break;
			}
		}
	}

	if (gpio_found && cam_res
		&& cam_res->shared_gpio_enabled) {
		struct list_head *list;
		struct cam_dev_res *dev_res = NULL;

		/* Count the dev number in the dev_list */
		list_for_each(list, &gpio_res->dev_list)
			dev_num++;

		/*
		 * Need free the gpio if only has last 1 device
		 * in the dev_list, otherwise, not free this
		 * gpio.
		 */
		if (dev_num == 1) {
			dev_res = list_first_entry(&gpio_res->dev_list,
				struct cam_dev_res, list);
			list_del_init(&dev_res->list);
			kfree(dev_res);
			list_del_init(&gpio_res->list);
			kfree(gpio_res);
		} else {
			list_for_each_entry(dev_res,
				&gpio_res->dev_list, list) {
				if (dev_res->dev == dev) {
					list_del_init(&dev_res->list);
					kfree(dev_res);
					need_free = false;
					break;
				}
			}
		}
	}

	if (need_free) {
		if (is_shared_pctrl_gpio) {
			pctrl_idx =
				cam_res_mgr_util_get_idx_from_shared_pctrl_gpio(
					gpio);
			cam_res_mgr_shared_pinctrl_select_state(
				pctrl_idx, false);
		}

		CAM_DBG(CAM_RES, "freeing gpio: %u", gpio);
		gpio_free(gpio);
	}

	mutex_unlock(&cam_res->gpio_res_lock);
}

void cam_res_mgr_gpio_free_arry(struct device *dev,
		const struct gpio *array, size_t num)
{
	while (num--)
		cam_res_mgr_gpio_free(dev, (array[num]).gpio);
}
EXPORT_SYMBOL(cam_res_mgr_gpio_free_arry);

int cam_res_mgr_gpio_set_value(unsigned int gpio, int value)
{
	int rc = 0;
	bool found = false;
	struct cam_gpio_res *gpio_res = NULL;

	mutex_lock(&cam_res->gpio_res_lock);
	if (cam_res && cam_res->shared_gpio_enabled) {
		list_for_each_entry(gpio_res, &cam_res->gpio_res_list, list) {
			if (gpio == gpio_res->gpio) {
				found = true;
				break;
			}
		}
	}

	/*
	 * Set the value directly if can't find the gpio from
	 * gpio_res_list, otherwise, need add ref count support
	 **/
	if (!found) {
		gpio_set_value_cansleep(gpio, value);
	} else {
		if (value) {
			gpio_res->power_on_count++;
			if (gpio_res->power_on_count < 2) {
				gpio_set_value_cansleep(gpio, value);
				CAM_DBG(CAM_RES,
					"Shared GPIO(%d) : HIGH", gpio);
			}
		} else {
			gpio_res->power_on_count--;
			if (gpio_res->power_on_count < 1) {
				gpio_set_value_cansleep(gpio, value);
				CAM_DBG(CAM_RES,
					"Shared GPIO(%d) : LOW", gpio);
			}
		}
	}

	mutex_unlock(&cam_res->gpio_res_lock);
	return rc;
}
EXPORT_SYMBOL(cam_res_mgr_gpio_set_value);

static int cam_res_mgr_shared_pinctrl_init(
	struct device *dev)
{
	int i = 0;
	char pctrl_active[50];
	char pctrl_suspend[50];
	struct cam_res_mgr_dt *dt = &cam_res->dt;

	cam_res->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(cam_res->pinctrl)) {
		CAM_ERR(CAM_RES, "Pinctrl not available");
		return -EINVAL;
	}

	for (i = 0; i < dt->num_shared_pctrl_gpio; i++) {
		memset(pctrl_active, '\0', sizeof(pctrl_active));
		memset(pctrl_suspend, '\0', sizeof(pctrl_suspend));
		snprintf(pctrl_active, sizeof(pctrl_active),
			"%s%s",
			cam_res->dt.pctrl_name[i],
			"_active");
		CAM_DBG(CAM_RES, "pctrl_active at index: %d name: %s",
			i, pctrl_active);
		snprintf(pctrl_suspend, sizeof(pctrl_suspend),
			"%s%s",
			cam_res->dt.pctrl_name[i],
			"_suspend");
		CAM_DBG(CAM_RES, "pctrl_suspend at index: %d name: %s",
			i, pctrl_suspend);
		cam_res->pctrl_res[i].active =
			pinctrl_lookup_state(cam_res->pinctrl,
			pctrl_active);
		if (IS_ERR_OR_NULL(cam_res->pctrl_res[i].active)) {
			CAM_ERR(CAM_RES,
				"Failed to get the active state pinctrl handle");
			return -EINVAL;
		}
		cam_res->pctrl_res[i].suspend =
			pinctrl_lookup_state(cam_res->pinctrl,
			pctrl_suspend);
		if (IS_ERR_OR_NULL(cam_res->pctrl_res[i].active)) {
			CAM_ERR(CAM_RES,
				"Failed to get the active state pinctrl handle");
			return -EINVAL;
		}
		cam_res->pctrl_res[i].pstatus = PINCTRL_STATUS_GOT;
	}

	cam_res->pstatus = PINCTRL_STATUS_GOT;

	return 0;
}

static int cam_res_mgr_parse_dt_shared_gpio(
	struct device *dev)
{
	int rc = 0;
	struct device_node *of_node = NULL;
	struct cam_res_mgr_dt *dt = &cam_res->dt;

	of_node = dev->of_node;
	dt->num_shared_gpio = of_property_count_u32_elems(of_node,
		"gpios-shared");

	if (dt->num_shared_gpio <= 0) {
		CAM_DBG(CAM_RES,
			"Not found any shared gpio");
		return -ENODEV;
	}

	if (dt->num_shared_gpio >= MAX_SHARED_GPIO_SIZE) {
		CAM_ERR(CAM_RES,
			"shared_gpio: %d max supported: %d",
			MAX_SHARED_GPIO_SIZE,
			dt->num_shared_gpio);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(of_node, "gpios-shared",
		dt->shared_gpio, dt->num_shared_gpio);
	if (rc) {
		CAM_ERR(CAM_RES, "Get shared gpio array failed.");
		return -EINVAL;
	}

	return rc;
}

static int cam_res_mgr_parse_dt_shared_pinctrl_gpio(
	struct device *dev)
{
	int rc = 0, i = 0;
	int pinctrl_name_nodes = 0;
	struct device_node *of_node = NULL;
	struct cam_res_mgr_dt *dt = &cam_res->dt;

	of_node = dev->of_node;
	dt->num_shared_pctrl_gpio = of_property_count_u32_elems(of_node,
		"gpios-shared-pinctrl");

	if (dt->num_shared_pctrl_gpio <= 0) {
		CAM_DBG(CAM_RES,
			"Not found any shared pinctrl res");
		return -ENODEV;
	}

	if (dt->num_shared_pctrl_gpio >= MAX_SHARED_PCTRL_GPIO_SIZE) {
		CAM_ERR(CAM_RES,
			"Invalid Pinctrl GPIO number %d. No shared gpio.",
			dt->num_shared_pctrl_gpio);
		return -EINVAL;
	}

	pinctrl_name_nodes = of_property_count_strings(of_node,
		"shared-pctrl-gpio-names");

	if (pinctrl_name_nodes != dt->num_shared_pctrl_gpio) {
		CAM_ERR(CAM_RES,
			"Mismatch between entries:: pctrl_gpio: %d and pctrl_name: %d",
			dt->num_shared_pctrl_gpio,
			pinctrl_name_nodes);
		return -EINVAL;
	}

	CAM_INFO(CAM_RES,
		"number of pctrl_gpio: %d", dt->num_shared_pctrl_gpio);

	rc = of_property_read_u32_array(of_node, "gpios-shared-pinctrl",
		dt->shared_pctrl_gpio, dt->num_shared_pctrl_gpio);
	if (rc) {
		CAM_ERR(CAM_RES, "Get shared pinctrl gpio array failed.");
		return -EINVAL;
	}

	for (i = 0; i < pinctrl_name_nodes; i++) {
		rc = of_property_read_string_index(of_node,
			"shared-pctrl-gpio-names",
			i, &(dt->pctrl_name[i]));
		CAM_INFO(CAM_RES, "shared-pctrl-gpio-names[%d] = %s",
			i, dt->pctrl_name[i]);
		if (rc) {
			CAM_ERR(CAM_RES,
				"i= %d pinctrl_name_nodes= %d reading clock-names failed",
				i, pinctrl_name_nodes);
			return rc;
		}
	}

	return rc;
}

static int cam_res_mgr_parse_dt(struct device *dev)
{
	int rc = 0;

	rc = cam_res_mgr_parse_dt_shared_gpio(dev);
	if (rc) {
		if (rc == -ENODEV) {
			CAM_DBG(CAM_RES,
				"Shared GPIO resources not available");
		} else {
			CAM_ERR(CAM_RES,
				"Shared gpio parsing failed: rc: %d", rc);
			return rc;
		}
	}

	rc = cam_res_mgr_parse_dt_shared_pinctrl_gpio(dev);
	if (rc) {
		if (rc == -ENODEV) {
			CAM_DBG(CAM_RES,
				"Pinctrl shared resources not available");
		} else {
			CAM_ERR(CAM_RES,  "Pinctrl parsing failed: rc: %d",
				rc);
			return rc;
		}
	} else {
		/* When shared pinctrl is detected do the init */
		rc = cam_res_mgr_shared_pinctrl_init(dev);
		if (rc) {
			CAM_ERR(CAM_RES, "Pinctrl init failed rc: %d", rc);
			return rc;
		}
	}

	return 0;
}

static int cam_res_mgr_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	int rc = 0;
	struct platform_device *pdev = to_platform_device(dev);

	cam_res = kzalloc(sizeof(*cam_res), GFP_KERNEL);
	if (!cam_res) {
		CAM_ERR(CAM_RES, "Not Enough Mem");
		return -ENOMEM;
	}
	cam_res->dev = &pdev->dev;

	CAM_DBG(CAM_RES, "ENTER");
	rc = cam_res_mgr_parse_dt(&pdev->dev);
	if (rc) {
		CAM_ERR(CAM_RES,
			"Error in parsing device tree, rc: %d", rc);
		kfree(cam_res);
		return rc;
	}

	if (cam_res->dt.num_shared_gpio || cam_res->dt.num_shared_pctrl_gpio) {
		CAM_DBG(CAM_RES, "Enable shared gpio support.");
		cam_res->shared_gpio_enabled = true;
	} else {
		CAM_DBG(CAM_RES, "Disable shared gpio support.");
		cam_res->shared_gpio_enabled = false;
	}

	mutex_init(&cam_res->flash_res_lock);
	mutex_init(&cam_res->gpio_res_lock);

	INIT_LIST_HEAD(&cam_res->gpio_res_list);
	INIT_LIST_HEAD(&cam_res->flash_res_list);

	CAM_DBG(CAM_RES, "Component bound successfully");
	return 0;
}

static void cam_res_mgr_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	if (cam_res) {
		cam_res_mgr_free_res();
		if (cam_res->pinctrl)
			devm_pinctrl_put(cam_res->pinctrl);
		cam_res->pinctrl = NULL;
		cam_res->pstatus = PINCTRL_STATUS_PUT;
		kfree(cam_res);
		cam_res = NULL;
	}

	CAM_DBG(CAM_RES, "Component unbound successfully");
}

const static struct component_ops cam_res_mgr_component_ops = {
	.bind = cam_res_mgr_component_bind,
	.unbind = cam_res_mgr_component_unbind,
};

static int cam_res_mgr_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_RES, "Adding Res mgr component");
	rc = component_add(&pdev->dev, &cam_res_mgr_component_ops);
	if (rc)
		CAM_ERR(CAM_RES, "failed to add component rc: %d", rc);

	return rc;
}

static int cam_res_mgr_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_res_mgr_component_ops);
	return 0;
}

static const struct of_device_id cam_res_mgr_dt_match[] = {
	{.compatible = "qcom,cam-res-mgr"},
	{}
};
MODULE_DEVICE_TABLE(of, cam_res_mgr_dt_match);

struct platform_driver cam_res_mgr_driver = {
	.probe = cam_res_mgr_probe,
	.remove = cam_res_mgr_remove,
	.driver = {
		.name = "cam_res_mgr",
		.owner = THIS_MODULE,
		.of_match_table = cam_res_mgr_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_res_mgr_init(void)
{
	return platform_driver_register(&cam_res_mgr_driver);
}

void cam_res_mgr_exit(void)
{
	platform_driver_unregister(&cam_res_mgr_driver);
}

MODULE_DESCRIPTION("Camera resource manager driver");
MODULE_LICENSE("GPL v2");
