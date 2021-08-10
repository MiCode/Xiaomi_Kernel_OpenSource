/*
 * cyttsp5_platform.c
 * Parade TrueTouch(TM) Standard Product V5 Platform Module.
 * For use with Parade touchscreen controllers.
 * Supported parts include:
 * CYTMA5XX
 * CYTMA448
 * CYTMA445A
 * CYTT21XXX
 * CYTT31XXX
 *
 * Copyright (C) 2015 Parade Technologies
 * Copyright (C) 2021 XiaoMi, Inc.
 * Copyright (C) 2013-2015 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Parade Technologies at www.paradetech.com <ttdrivers@paradetech.com>
 *
 */

#include "cyttsp5_regs.h"
#include "cyttsp5_platform.h"

#ifdef CYTTSP5_PLATFORM_FW_UPGRADE
/* FW for Panel ID = 0x00 */
#include "cyttsp5_fw_pid00.h"
static struct cyttsp5_touch_firmware cyttsp5_firmware_pid00 = {
	.img = cyttsp4_img_pid00,
	.size = ARRAY_SIZE(cyttsp4_img_pid00),
	.ver = cyttsp4_ver_pid00,
	.vsize = ARRAY_SIZE(cyttsp4_ver_pid00),
	.panel_id = 0x00,
};

/* FW for Panel ID = 0x01 */
#include "cyttsp5_fw_pid01.h"
static struct cyttsp5_touch_firmware cyttsp5_firmware_pid01 = {
	.img = cyttsp4_img_pid01,
	.size = ARRAY_SIZE(cyttsp4_img_pid01),
	.ver = cyttsp4_ver_pid01,
	.vsize = ARRAY_SIZE(cyttsp4_ver_pid01),
	.panel_id = 0x01,
};

/* FW for Panel ID not enabled (legacy) */
#include "cyttsp5_fw.h"
static struct cyttsp5_touch_firmware cyttsp5_firmware = {
	.img = cyttsp4_img,
	.size = ARRAY_SIZE(cyttsp4_img),
	.ver = cyttsp4_ver,
	.vsize = ARRAY_SIZE(cyttsp4_ver),
};
#else
/* FW for Panel ID not enabled (legacy) */
static struct cyttsp5_touch_firmware cyttsp5_firmware = {
	.img = NULL,
	.size = 0,
	.ver = NULL,
	.vsize = 0,
};
#endif

#ifdef CYTTSP5_PLATFORM_TTCONFIG_UPGRADE
/* TT Config for Panel ID = 0x00 */
#include "cyttsp5_params_pid00.h"
static struct touch_settings cyttsp5_sett_param_regs_pid00 = {
	.data = (uint8_t *)&cyttsp4_param_regs_pid00[0],
	.size = ARRAY_SIZE(cyttsp4_param_regs_pid00),
	.tag = 0,
};

static struct touch_settings cyttsp5_sett_param_size_pid00 = {
	.data = (uint8_t *)&cyttsp4_param_size_pid00[0],
	.size = ARRAY_SIZE(cyttsp4_param_size_pid00),
	.tag = 0,
};

static struct cyttsp5_touch_config cyttsp5_ttconfig_pid00 = {
	.param_regs = &cyttsp5_sett_param_regs_pid00,
	.param_size = &cyttsp5_sett_param_size_pid00,
	.fw_ver = ttconfig_fw_ver_pid00,
	.fw_vsize = ARRAY_SIZE(ttconfig_fw_ver_pid00),
	.panel_id = 0x00,
};

/* TT Config for Panel ID = 0x01 */
#include "cyttsp5_params_pid01.h"
static struct touch_settings cyttsp5_sett_param_regs_pid01 = {
	.data = (uint8_t *)&cyttsp4_param_regs_pid01[0],
	.size = ARRAY_SIZE(cyttsp4_param_regs_pid01),
	.tag = 0,
};

static struct touch_settings cyttsp5_sett_param_size_pid01 = {
	.data = (uint8_t *)&cyttsp4_param_size_pid01[0],
	.size = ARRAY_SIZE(cyttsp4_param_size_pid01),
	.tag = 0,
};

static struct cyttsp5_touch_config cyttsp5_ttconfig_pid01 = {
	.param_regs = &cyttsp5_sett_param_regs_pid01,
	.param_size = &cyttsp5_sett_param_size_pid01,
	.fw_ver = ttconfig_fw_ver_pid01,
	.fw_vsize = ARRAY_SIZE(ttconfig_fw_ver_pid01),
	.panel_id = 0x01,
};

/* TT Config for Panel ID not enabled (legacy)*/
#include "cyttsp5_params.h"
static struct touch_settings cyttsp5_sett_param_regs = {
	.data = (uint8_t *)&cyttsp4_param_regs[0],
	.size = ARRAY_SIZE(cyttsp4_param_regs),
	.tag = 0,
};

static struct touch_settings cyttsp5_sett_param_size = {
	.data = (uint8_t *)&cyttsp4_param_size[0],
	.size = ARRAY_SIZE(cyttsp4_param_size),
	.tag = 0,
};

static struct cyttsp5_touch_config cyttsp5_ttconfig = {
	.param_regs = &cyttsp5_sett_param_regs,
	.param_size = &cyttsp5_sett_param_size,
	.fw_ver = ttconfig_fw_ver,
	.fw_vsize = ARRAY_SIZE(ttconfig_fw_ver),
};
#else
/* TT Config for Panel ID not enabled (legacy)*/
static struct cyttsp5_touch_config cyttsp5_ttconfig = {
	.param_regs = NULL,
	.param_size = NULL,
	.fw_ver = NULL,
	.fw_vsize = 0,
};
#endif

static struct cyttsp5_touch_firmware *cyttsp5_firmwares[] = {
#ifdef CYTTSP5_PLATFORM_FW_UPGRADE
	&cyttsp5_firmware_pid00,
	&cyttsp5_firmware_pid01,
#endif
	NULL, /* Last item should always be NULL */
};

static struct cyttsp5_touch_config *cyttsp5_ttconfigs[] = {
#ifdef CYTTSP5_PLATFORM_TTCONFIG_UPGRADE
	&cyttsp5_ttconfig_pid00,
	&cyttsp5_ttconfig_pid01,
#endif
	NULL, /* Last item should always be NULL */
};

struct cyttsp5_loader_platform_data _cyttsp5_loader_platform_data = {
	.fw = &cyttsp5_firmware,
	.ttconfig = &cyttsp5_ttconfig,
	.fws = cyttsp5_firmwares,
	.ttconfigs = cyttsp5_ttconfigs,
	.flags = CY_LOADER_FLAG_NONE,
};

int cyttsp5_xres(struct cyttsp5_core_platform_data *pdata,
		struct device *dev)
{
	int rst_gpio = pdata->rst_gpio;
	int rc = 0;

	gpio_set_value(rst_gpio, 1);
	msleep(3);
	gpio_set_value(rst_gpio, 0);
	msleep(1);
	gpio_set_value(rst_gpio, 1);
	msleep(5);
	dev_info(dev,
		"%s: RESET CYTTSP gpio=%d r=%d\n", __func__,
		pdata->rst_gpio, rc);
	return rc;
}

static int cyttsp5_reg_enable(struct device *dev, bool enable)
{
	int rc = 0;
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);

	if (!enable) {
		goto disable_reg;
	}

	if (cd->avdd_reg) {
		rc = regulator_enable(cd->avdd_reg);
		if (rc < 0) {
			pr_err("%s: Failed to enable pwr regulator\n", __func__);
			goto avdd_failed;
		}
	}

	if (cd->vdd_reg) {
		rc = regulator_enable(cd->vdd_reg);
		if (rc < 0) {
			pr_err("%s: Failed to enable bus regulator\n", __func__);
			goto vdd_failed;
		}
	}

	return rc;

disable_reg:
	if (cd->vdd_reg)
		regulator_disable(cd->vdd_reg);
vdd_failed:
	if (cd->avdd_reg)
		regulator_disable(cd->avdd_reg);
avdd_failed:
	return rc;
}

static int cyttsp5_reg_init(struct device *dev, bool on)
{
	int rc = 0;
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);

	if (!on)
		goto regulator_put;

	cd->avdd_reg = regulator_get(dev, "avdd");
	if (IS_ERR(cd->avdd_reg)) {
		pr_err("%s: Failed to get power regulator\n", __func__);
		rc = PTR_ERR(cd->avdd_reg);
		goto regulator_put;
	}

	cd->vdd_reg = regulator_get(dev, "vdd");
	if (IS_ERR(cd->vdd_reg)) {
		pr_err("%s: Failed to get bus pullup regulator\n", __func__);
		rc = PTR_ERR(cd->vdd_reg);
		goto regulator_put;
	}

	rc = cyttsp5_reg_enable(dev, true);
	if (rc < 0) {
		pr_err("%s: Failed to enable power!\n", __func__);
	}

	return rc;

regulator_put:
	rc = cyttsp5_reg_enable(dev, false);
	if (rc < 0) {
		pr_err("%s: Failed to disable power!\n", __func__);
		return rc;
	}

	if (cd->vdd_reg) {
		regulator_put(cd->vdd_reg);
		cd->vdd_reg = NULL;
	}

	if (cd->avdd_reg) {
		regulator_put(cd->avdd_reg);
		cd->avdd_reg = NULL;
	}

	return rc;
}

static int cyttsp5_gpio_init(struct cyttsp5_core_platform_data *pdata, bool enable)
{
	int rc = 0;
	int rst_gpio = pdata->rst_gpio;
	int irq_gpio = pdata->irq_gpio;

	if (!enable)
		goto free_irq_gpio;

	rc = gpio_request(rst_gpio, "cyttsp5_rest_gpio");
	if (rc < 0) {
		pr_err("%s: Failed to request reset gpio:%d\n", __func__, rst_gpio);
		goto fail;
	}
	rc = gpio_direction_output(rst_gpio, 0);
	if (rc < 0) {
		pr_err("%s: Failed to set gpio %d direction\n", __func__, rst_gpio);
		goto free_rst_gpio;
	}

	rc = gpio_request(irq_gpio, "cyttsp5_irq_gpio");
	if (rc < 0) {
		pr_err("%s: Failed to request irq gpio:%d\n", __func__, irq_gpio);
		goto free_rst_gpio;
	}
	rc = gpio_direction_input(irq_gpio);
	if (rc < 0) {
		pr_err("%s: Failed to set gpio %d direction\n", __func__, irq_gpio);
		goto free_irq_gpio;
	}

	return rc;

free_irq_gpio:
	gpio_free(irq_gpio);
free_rst_gpio:
	gpio_free(rst_gpio);
fail:
	return rc;
}

static int cyttsp5_pinctrl_init(struct device *dev, bool enable)
{
	int rc = 0;
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);

	if (!enable)
		goto err_pinctrl_lookup;

	cd->ts_pinctrl = devm_pinctrl_get(dev);

	if (IS_ERR_OR_NULL(cd->ts_pinctrl)) {
		rc = PTR_ERR(cd->ts_pinctrl);
		dev_err(dev, "Target does not use pinctrl %d\n", rc);
		goto err_pinctrl_get;
	}

	cd->pinctrl_state_active =
		pinctrl_lookup_state(cd->ts_pinctrl, PINCTRL_STATE_ACTIVE);

	if (IS_ERR_OR_NULL(cd->pinctrl_state_active)) {
		rc = PTR_ERR(cd->pinctrl_state_active);
		dev_err(dev, "Can not lookup %s pinstate %d\n",
				PINCTRL_STATE_ACTIVE, rc);
		goto err_pinctrl_lookup;
	}

	cd->pinctrl_state_suspend =
		pinctrl_lookup_state(cd->ts_pinctrl, PINCTRL_STATE_SUSPEND);

	if (IS_ERR_OR_NULL(cd->pinctrl_state_suspend)) {
		rc = PTR_ERR(cd->pinctrl_state_suspend);
		dev_err(dev, "Can not lookup %s pinstate %d\n",
				PINCTRL_STATE_SUSPEND, rc);
		goto err_pinctrl_lookup;
	}

	if (cd->ts_pinctrl) {
		rc = pinctrl_select_state(cd->ts_pinctrl,
				cd->pinctrl_state_active);
		if (rc < 0) {
			dev_err(dev, "%s: Failed to select %s pinstate %d\n",
					__func__, PINCTRL_STATE_ACTIVE, rc);
			goto err_pinctrl_lookup;
		}
	}
	return rc;
err_pinctrl_lookup:
	devm_pinctrl_put(cd->ts_pinctrl);
err_pinctrl_get:
	cd->ts_pinctrl = NULL;
	return rc;
}

int cyttsp5_init(struct cyttsp5_core_platform_data *pdata,
		int on, struct device *dev)
{
	int rc = 0;
	if (on) {
		rc = cyttsp5_pinctrl_init(dev, true);
		if (rc < 0) {
			pr_err("%s: Failed to init pinctrl\n", __func__);
			return rc;
		}
		rc = cyttsp5_gpio_init(pdata, true);
		if (rc < 0) {
			pr_err("%s: Failed to init gpio\n", __func__);
			return rc;
		}
		rc = cyttsp5_reg_init(dev, true);
		if (rc < 0) {
			pr_err("%s: Failed to init regulator\n", __func__);
			return rc;
		}
		pr_info("%s: cyttsp5 pinctrl/gpio/regulator init successful!", __func__);
	} else {
		rc = cyttsp5_gpio_init(pdata, false);
		if (rc < 0) {
			pr_err("%s: Failed to deinit gpio!\n", __func__);
			return rc;
		}

		rc = cyttsp5_pinctrl_init(dev, false);
		if (rc < 0) {
			pr_err("%s: Failed to deinit pinctrl!\n", __func__);
			return rc;
		}
		rc = cyttsp5_reg_init(dev, false);
		if (rc < 0) {
			pr_err("%s: Failed to deinit regulator!\n", __func__);
			return rc;
		}
		pr_info("%s: cyttsp5 pinctrl/gpio/regulator deinit successful!", __func__);
	}
	return rc;
}

static int cyttsp5_wakeup(struct cyttsp5_core_platform_data *pdata,
		struct device *dev, atomic_t *ignore_irq)
{
	int rc = 0;
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);

	dev_info(dev, "%s: enable regulator!", __func__);
	if (cd->ts_pinctrl) {
		rc = pinctrl_select_state(cd->ts_pinctrl,
				cd->pinctrl_state_active);
		if (rc < 0) {
			dev_err(dev, "%s: Failed to select %s pinstate %d\n",
					__func__, PINCTRL_STATE_ACTIVE, rc);
			goto out;
		}
	}

	rc = cyttsp5_reg_enable(dev, true);
	if (rc < 0) {
		dev_err(dev, "%s: enable regulator failed,r=%d\n", __func__, rc);
		goto out;
	}
	rc = cyttsp5_xres(pdata, dev);
	if (rc < 0) {
		dev_err(dev, "%s: reset hw failed,r=%d\n", __func__, rc);
		goto out;
	}
out:
	return rc;
}

static int cyttsp5_sleep(struct cyttsp5_core_platform_data *pdata,
		struct device *dev, atomic_t *ignore_irq)
{
	int rc = 0;
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);

	gpio_direction_output(pdata->rst_gpio, 0);
	dev_info(dev, "%s: disable regulator!", __func__);
	rc = cyttsp5_reg_enable(dev, false);
	if (rc < 0)
		goto out;

	if (cd->ts_pinctrl) {
		rc = pinctrl_select_state(cd->ts_pinctrl,
				cd->pinctrl_state_suspend);
		if (rc < 0) {
			dev_err(dev, "%s: Failed to select %s pinstate %d\n",
					__func__, PINCTRL_STATE_SUSPEND, rc);
		}
	}
out:
	return rc;
}

int cyttsp5_power(struct cyttsp5_core_platform_data *pdata,
		int on, struct device *dev, atomic_t *ignore_irq)
{
	if (on)
		return cyttsp5_wakeup(pdata, dev, ignore_irq);

	return cyttsp5_sleep(pdata, dev, ignore_irq);
}

int cyttsp5_irq_stat(struct cyttsp5_core_platform_data *pdata,
		struct device *dev)
{
	return gpio_get_value(pdata->irq_gpio);
}

#ifdef CYTTSP5_DETECT_HW
int cyttsp5_detect(struct cyttsp5_core_platform_data *pdata,
		struct device *dev, cyttsp5_platform_read read)
{
	int retry = 3;
	int rc;
	char buf[1];

	while (retry--) {
		/* Perform reset, wait for 100 ms and perform read */
		parade_debug(dev, DEBUG_LEVEL_2, "%s: Performing a reset\n",
			__func__);
		pdata->xres(pdata, dev);
		msleep(100);
		rc = read(dev, buf, 1);
		if (!rc)
			return 0;

		parade_debug(dev, DEBUG_LEVEL_2, "%s: Read unsuccessful, try=%d\n",
			__func__, 3 - retry);
	}

	return rc;
}
#endif
