/*
 * FPC1020 Fingerprint sensor device driver
 *
 * This driver will control the platform resources that the FPC fingerprint
 * sensor needs to operate. The major things are probing the sensor to check
 * that it is actually connected and let the Kernel know this and with that also
 * enabling and disabling of regulators, controlling GPIOs such as sensor reset
 * line, sensor IRQ line.
 *
 * The driver will expose most of its available functionality in sysfs which
 * enables dynamic control of these features from eg. a user space process.
 *
 * The sensor's IRQ events will be pushed to Kernel's event handling system and
 * are exposed in the drivers event node.
 *
 * This driver will NOT send any commands to the sensor it only controls the
 * electrical parts.
 *
 *
 * Copyright (c) 2015 Fingerprint Cards AB <tech@fingerprints.com>
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_wakeup.h>
#include <linux/fb.h>
#include <linux/bitmap.h>

#define FPC_TTW_HOLD_TIME 1000

#define RESET_LOW_SLEEP_MIN_US 5000
#define RESET_LOW_SLEEP_MAX_US (RESET_LOW_SLEEP_MIN_US + 100)
#define RESET_HIGH_SLEEP1_MIN_US 100
#define RESET_HIGH_SLEEP1_MAX_US (RESET_HIGH_SLEEP1_MIN_US + 100)
#define RESET_HIGH_SLEEP2_MIN_US 5000
#define RESET_HIGH_SLEEP2_MAX_US (RESET_HIGH_SLEEP2_MIN_US + 100)
#define PWR_ON_SLEEP_MIN_US 100
#define PWR_ON_SLEEP_MAX_US (PWR_ON_SLEEP_MIN_US + 900)

//for optical
#define OPTICAL_PWR_ON_SLEEP_MIN_US 1000
#define OPTICAL_PWR_ON_SLEEP_MAX_US (OPTICAL_PWR_ON_SLEEP_MIN_US + 100)

#define OPTICAL_RST_SLEEP_MIN_US 1000
#define OPTICAL_RST_SLEEP_MAX_US (OPTICAL_RST_SLEEP_MIN_US + 100)

#define NUM_PARAMS_REG_ENABLE_SET 2

#define RELEASE_WAKELOCK_W_V "release_wakelock_with_verification"
#define RELEASE_WAKELOCK "release_wakelock"
#define START_IRQS_RECEIVED_CNT "start_irqs_received_counter"

static const char * const pctl_names[] = {
	"fpc1020_reset_reset",
	"fpc1020_reset_active",
	"fpc1020_irq_active",
	"fpc1020_vdd1v2_default",
	"fpc1020_vdd1v8_default",
};

struct vreg_config {
	char *name;
	unsigned long vmin;
	unsigned long vmax;
	int ua_load;
	int gpio;
};

static struct vreg_config vreg_conf[] = {
	{ "vdd_spi", 1200000UL, 1200000UL, 6000, 0,},
	{ "vdd_io",  1800000UL, 1800000UL, 6000, 0,},
	{ "vdd_ana", 2800000UL, 2800000UL, 6000, 0,},
};

struct fpc1020_data {
	struct device *dev;
	struct pinctrl *fingerprint_pinctrl;
	struct pinctrl_state *pinctrl_state[ARRAY_SIZE(pctl_names)];
	struct regulator *vreg[ARRAY_SIZE(vreg_conf)];
	struct wakeup_source ttw_wl;
	struct mutex lock; /* To set/get exported values in sysfs */
	int irq_gpio;
	int rst_gpio;

	int vdd1v2_gpio;
	int vdd1v8_gpio;
	int vdd2v8_gpio;
	bool gpios_requested;

	int nbr_irqs_received;
	int nbr_irqs_received_counter_start;
	bool prepared;
#ifdef CONFIG_FPC_COMPAT
	bool compatible_enabled;
#endif
	atomic_t wakeup_enabled; /* Used both in ISR and non-ISR */
};

struct fpc_hotzone_setting {
	/*hotzone setting:
	  left,right,top,bottom
	  */
	uint32_t left;
	uint32_t right;
	uint32_t top;
	uint32_t bottom;
	/*indicate the hotzone will reset*/
	uint32_t update;
};
typedef struct fpc_hotzone_setting fpc_hotzone_setting_t;
static struct fpc1020_data *fpc_data_bak;
volatile long unsigned int finger_irq_value;


/** hotzone_value_set
 *this function will set the hotzone postion in kernel driver
 */
static struct fpc_hotzone_setting hotzone_setting;

/*define the hoztone with MACRO */
#define TOUCHSCREEN_BOUNDARY_LOW_X       hotzone_setting.left
#define TOUCHSCREEN_BOUNDARY_LOW_Y      hotzone_setting.top
#define TOUCHSCREEN_BOUNDARY_HIGH_X     hotzone_setting.right
#define TOUCHSCREEN_BOUNDARY_HIGH_Y     hotzone_setting.bottom

/*the pressure, size thresh*/
#define TOUCHSCREEN_PRESSURE_THRESH    0x30
#define TOUCHSCREEN_AREA_THRESH    0x11f

#define HOTZONE_IN 1
#define HOTZONE_OUT 0
volatile long unsigned int simulate_irq_value;
static int request_vreg_gpio(struct fpc1020_data *fpc1020, bool enable);

static int vreg_setup(struct fpc1020_data *fpc1020, const char *name, bool enable);
static int device_prepare(struct fpc1020_data *fpc1020, bool enable);
static irqreturn_t fpc1020_simulate_irq(struct device *dev);

#ifdef CONFIG_FPC_COMPAT
static irqreturn_t fpc1020_irq_handler(int irq, void *handle);
#endif
static int fpc1020_request_named_gpio(struct fpc1020_data *fpc1020,
		const char *label, int *gpio);
static int hw_reset(struct  fpc1020_data *fpc1020);


static int request_vreg_gpio(struct fpc1020_data *fpc1020, bool enable)
{

	int rc=0;
	struct device *dev = fpc1020->dev;
	dev_err(dev, "fpc %s: enter!\n", __func__);

	mutex_lock(&fpc1020->lock);

	if (enable && !fpc1020->gpios_requested) {
		rc = fpc1020_request_named_gpio(fpc1020, "fpc,gpio_vdd1v2", &fpc1020->vdd1v2_gpio);
		if (rc) {
			dev_err(dev, "fpc vdd1v2 gpio request failed\n");
			goto release;
		} else {
			vreg_conf[0].gpio = fpc1020->vdd1v2_gpio;
			dev_info(dev, "fpc vdd1v2 gpio applied at  %d\n", fpc1020->vdd1v2_gpio);
		}

		rc = fpc1020_request_named_gpio(fpc1020, "fpc,gpio_vdd1v8", &fpc1020->vdd1v8_gpio);
		if (rc) {
			dev_err(dev, "fpc vdd1v2 gpio request failed\n");
			goto release;
		} else {
			vreg_conf[1].gpio = fpc1020->vdd1v8_gpio;
			dev_info(dev, "fpc vdd1v8 gpio applied at  %d\n", fpc1020->vdd1v8_gpio);
		}

		rc = fpc1020_request_named_gpio(fpc1020, "fpc,gpio_vdd2v8", &fpc1020->vdd2v8_gpio);
		if (rc) {
			dev_err(dev, "fpc vdd2v8 gpio request failed\n");
			goto release;
		} else {
			vreg_conf[2].gpio = fpc1020->vdd2v8_gpio;
			dev_info(dev, "fpc vdd2v8 gpio applied at  %d\n", fpc1020->vdd2v8_gpio);
		}

		fpc1020->gpios_requested = true;
		dev_dbg(dev, "vreg gpio requested successfully!\n");
		goto exit;
	} else if (!enable && fpc1020->gpios_requested) {
release:
		if (gpio_is_valid(fpc1020->vdd1v2_gpio)) {
			devm_gpio_free(dev, fpc1020->vdd1v2_gpio);
			fpc1020->vdd1v2_gpio = -1;
		}

		if (gpio_is_valid(fpc1020->vdd1v8_gpio)) {
			devm_gpio_free(dev, fpc1020->vdd1v8_gpio);
			fpc1020->vdd1v8_gpio = -1;
		}

		if (gpio_is_valid(fpc1020->vdd2v8_gpio)) {
			devm_gpio_free(dev, fpc1020->vdd2v8_gpio);
			fpc1020->vdd2v8_gpio = -1;
		}

		vreg_conf[0].gpio = 0;
		vreg_conf[1].gpio = 0;
		vreg_conf[2].gpio = 0;
		fpc1020->gpios_requested = false;
	} else {
		dev_info(dev, "%s: enable: %d, gpios_requested: %d ???\n",
				__func__, enable, fpc1020->gpios_requested);
	}

exit:
	mutex_unlock(&fpc1020->lock);
	dev_dbg(dev, "fpc %s: exit!\n", __func__);
	return rc;
}

static int vreg_setup(struct fpc1020_data *fpc1020, const char *name,
		bool enable)
{
	size_t i;
	int rc = 0;
	int gpio = 0;
	struct device *dev = fpc1020->dev;

	dev_err(dev, "fpc %s: enter!\n", __func__);

	for (i = 0; i < ARRAY_SIZE(vreg_conf); i++) {
		const char *n = vreg_conf[i].name;

		if (!memcmp(n, name, strlen(n)))
			goto found;
	}

	dev_err(dev, "fpc %s: Regulator %s not found\n", __func__, name);

	return -EINVAL;

found:
	gpio = vreg_conf[i].gpio;
	if (enable) {
		if (gpio_is_valid(gpio)) {
			rc = gpio_direction_output(gpio, 1);
			if (rc) {
				dev_err(dev, "fpc %s: fail to set gpio %d !\n", __func__, gpio);
				return rc;
			}
		} else {
			dev_err(dev, "fpc %s: unable to get gpio %d!\n", __func__, gpio);
			return -EINVAL;
		}
	} else {
		if (gpio_is_valid(gpio)) {
			rc = gpio_direction_output(gpio, 0);
			if (rc) {
				dev_err(dev, "fpc %s: fail to clear gpio %d!\n", __func__, gpio);
				return rc;
			}
		} else {
			dev_err(dev, "fpc %s: unable to get gpio %d!\n", __func__, gpio);
			return -EINVAL;
		}
	}
	return rc;
}


/*
 * sysfs node for release GPIO.
 *
 */

static ssize_t request_vreg_gpio_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);

	if (!memcmp(buf, "enable", strlen("enable")))
		rc = request_vreg_gpio(fpc1020, true);
	else if (!memcmp(buf, "disable", strlen("disable")))
		rc = request_vreg_gpio(fpc1020, false);
	else
		return -EINVAL;

	return rc ? rc : count;
}
static DEVICE_ATTR(request_vreg, 0200, NULL, request_vreg_gpio_set);



/*
 * sysfs node for controlling clocks.
 *
 * This is disabled in platform variant of this driver but kept for
 * backwards compatibility. Only prints a debug print that it is
 * disabled.
 */
static ssize_t clk_enable_set(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	dev_dbg(dev, "clk_enable sysfs node not enabled in platform driver\n");
	return count;
}
static DEVICE_ATTR(clk_enable, 0200, NULL, clk_enable_set);

/*
 * Will try to select the set of pins (GPIOS) defined in a pin control node of
 * the device tree named @p name.
 *
 * The node can contain several eg. GPIOs that is controlled when selecting it.
 * The node may activate or deactivate the pins it contains, the action is
 * defined in the device tree node itself and not here. The states used
 * internally is fetched at probe time.
 *
 * @see pctl_names
 * @see fpc1020_probe
 */
static int select_pin_ctl(struct fpc1020_data *fpc1020, const char *name)
{
	size_t i;
	int rc;
	struct device *dev = fpc1020->dev;

	for (i = 0; i < ARRAY_SIZE(pctl_names); i++) {
		const char *n = pctl_names[i];

		if (!memcmp(n, name, strlen(n))) {
			rc = pinctrl_select_state(fpc1020->fingerprint_pinctrl,
					fpc1020->pinctrl_state[i]);
			if (rc)
				dev_err(dev, "fpc %s: cannot select '%s'\n", __func__, name);
			else
				dev_dbg(dev, "fpc %s: selected '%s'\n", __func__, name);
			goto exit;
		}
	}

	rc = -EINVAL;
	dev_err(dev, "%s:'%s' not found gpio\n", __func__, name);

exit:
	return rc;
}

static ssize_t pinctl_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	int rc;

	mutex_lock(&fpc1020->lock);
	rc = select_pin_ctl(fpc1020, buf);
	mutex_unlock(&fpc1020->lock);

	return rc ? rc : count;
}
static DEVICE_ATTR(pinctl_set, 0200, NULL, pinctl_set);

static ssize_t regulator_enable_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	char op;
	char name[16];
	int rc;
	bool enable;

	if (sscanf(buf, "%15[^,],%c", name, &op) != NUM_PARAMS_REG_ENABLE_SET)
		return -EINVAL;
	if (op == 'e')
		enable = true;
	else if (op == 'd')
		enable = false;
	else
		return -EINVAL;

	mutex_lock(&fpc1020->lock);
	rc = vreg_setup(fpc1020, name, enable);
	mutex_unlock(&fpc1020->lock);

	return rc ? rc : count;
}
static DEVICE_ATTR(regulator_enable, 0200, NULL, regulator_enable_set);

static int hw_reset(struct fpc1020_data *fpc1020)
{
	//int irq_gpio;
	int rc;

	rc = select_pin_ctl(fpc1020, "fpc1020_reset_active");
	if (rc)
		goto exit;

	usleep_range(OPTICAL_RST_SLEEP_MIN_US, OPTICAL_PWR_ON_SLEEP_MAX_US);

	rc = select_pin_ctl(fpc1020, "fpc1020_reset_reset");
	if (rc)
		goto exit;

	usleep_range(OPTICAL_RST_SLEEP_MIN_US, OPTICAL_PWR_ON_SLEEP_MAX_US);

	rc = select_pin_ctl(fpc1020, "fpc1020_reset_active");
	if (rc)
		goto exit;

	usleep_range(OPTICAL_RST_SLEEP_MIN_US, OPTICAL_PWR_ON_SLEEP_MAX_US);
	//irq_gpio = gpio_get_value(fpc1020->irq_gpio);

	//dev_info("fpc1020.irq_gpio is %d\n", fpc1020->irq_gpio);
	//dev_info("fpc IRQ after reset %d\n", irq_gpio);

exit:
	return rc;
}

static ssize_t hw_reset_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc = -EINVAL;
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);

	if (!memcmp(buf, "reset", strlen("reset"))) {
		mutex_lock(&fpc1020->lock);
		rc = hw_reset(fpc1020);
		mutex_unlock(&fpc1020->lock);
	} else {
		return rc;
	}
	return rc ? rc : count;
}
static DEVICE_ATTR(hw_reset, 0200, NULL, hw_reset_set);

/*
 * Will setup GPIOs, and regulators to correctly initialize the touch sensor to
 * be ready for work.
 *
 * In the correct order according to the sensor spec this function will
 * enable/disable regulators, and reset line, all to set the sensor in a
 * correct power on or off state "electrical" wise.
 *
 * @see  device_prepare_set
 * @note This function will not send any commands to the sensor it will only
 *       control it "electrically".
 */
static int device_prepare(struct fpc1020_data *fpc1020, bool enable)
{
	int rc = 0;
	struct device *dev = fpc1020->dev;

	mutex_lock(&fpc1020->lock);

	dev_err(dev, "fpc device prepare enter.\n");

	if (enable && !fpc1020->prepared) {
		fpc1020->prepared = true;
		select_pin_ctl(fpc1020, "fpc1020_reset_reset");

		//1.2V
		rc = vreg_setup(fpc1020, "vdd_spi", true);
		if (rc)
			goto exit;

		usleep_range(OPTICAL_PWR_ON_SLEEP_MIN_US, OPTICAL_PWR_ON_SLEEP_MAX_US);

		//1.8V
		rc = vreg_setup(fpc1020, "vdd_io", true);
		if (rc)
			goto exit_1;

		usleep_range(OPTICAL_PWR_ON_SLEEP_MIN_US, OPTICAL_PWR_ON_SLEEP_MAX_US);

		//2.8V
		rc = vreg_setup(fpc1020, "vdd_ana", true);
		if (rc)
			goto exit_2;

		usleep_range(OPTICAL_PWR_ON_SLEEP_MIN_US * 2, OPTICAL_PWR_ON_SLEEP_MAX_US * 2);

		/* As we can't control chip select here the other part of the
		 * sensor driver eg. the TEE driver needs to do a _SOFT_ reset
		 * on the sensor after power up to be sure that the sensor is
		 * in a good state after power up. Okeyed by ASIC. */

		(void)select_pin_ctl(fpc1020, "fpc1020_reset_active");
	} else if (!enable && fpc1020->prepared) {
		rc = 0;
		(void)select_pin_ctl(fpc1020, "fpc1020_reset_reset");

		usleep_range(PWR_ON_SLEEP_MIN_US, PWR_ON_SLEEP_MAX_US);

		(void)vreg_setup(fpc1020, "vdd_ana", false);
exit_2:
		(void)vreg_setup(fpc1020, "vdd_io", false);
exit_1:
		(void)vreg_setup(fpc1020, "vdd_spi", false);
exit:
		fpc1020->prepared = false;
	}

	mutex_unlock(&fpc1020->lock);

	return rc;
}

/*
 * sysfs node to enable/disable (power up/power down) the touch sensor
 *
 * @see device_prepare
 */
static ssize_t device_prepare_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);

	if (!memcmp(buf, "enable", strlen("enable")))
		rc = device_prepare(fpc1020, true);
	else if (!memcmp(buf, "disable", strlen("disable")))
		rc = device_prepare(fpc1020, false);
	else
		return -EINVAL;

	return rc ? rc : count;
}
static DEVICE_ATTR(device_prepare, 0200, NULL, device_prepare_set);

/**
 * sysfs node for controlling whether the driver is allowed
 * to wake up the platform on interrupt.
 */
static ssize_t wakeup_enable_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	ssize_t ret = count;

	mutex_lock(&fpc1020->lock);
	if (!memcmp(buf, "enable", strlen("enable")))
		atomic_set(&fpc1020->wakeup_enabled, 1);
	else if (!memcmp(buf, "disable", strlen("disable")))
		atomic_set(&fpc1020->wakeup_enabled, 0);
	else
		ret = -EINVAL;
	mutex_unlock(&fpc1020->lock);

	return ret;
}
static DEVICE_ATTR(wakeup_enable, 0200, NULL, wakeup_enable_set);


/*
 * sysfs node for controlling the wakelock.
 */
static ssize_t handle_wakelock_cmd(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct  fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	ssize_t ret = count;

	mutex_lock(&fpc1020->lock);
	if (!memcmp(buf, RELEASE_WAKELOCK_W_V,
				min(count, strlen(RELEASE_WAKELOCK_W_V)))) {
		if (fpc1020->nbr_irqs_received_counter_start ==
				fpc1020->nbr_irqs_received) {
			__pm_relax(&fpc1020->ttw_wl);
		} else {
			dev_dbg(dev, "Ignore releasing of wakelock %d != %d",
					fpc1020->nbr_irqs_received_counter_start,
					fpc1020->nbr_irqs_received);
		}
	} else if (!memcmp(buf, RELEASE_WAKELOCK, min(count,
					strlen(RELEASE_WAKELOCK)))) {
		__pm_relax(&fpc1020->ttw_wl);
	} else if (!memcmp(buf, START_IRQS_RECEIVED_CNT,
				min(count, strlen(START_IRQS_RECEIVED_CNT)))) {
		fpc1020->nbr_irqs_received_counter_start =
			fpc1020->nbr_irqs_received;
	} else
		ret = -EINVAL;
	mutex_unlock(&fpc1020->lock);

	return ret;
}
static DEVICE_ATTR(handle_wakelock, 0200, NULL, handle_wakelock_cmd);

/*
 * sysf node to check the interrupt status of the sensor, the interrupt
 * handler should perform sysf_notify to allow userland to poll the node.
 */
static ssize_t irq_get(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	int irq = gpio_get_value(fpc1020->irq_gpio);

	return scnprintf(buf, PAGE_SIZE, "%i\n", irq);
}

/*
 * writing to the irq node will just drop a printk message
 * and return success, used for latency measurement.
 */
static ssize_t irq_ack(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);

	dev_dbg(fpc1020->dev, "%s\n", __func__);

	return count;
}
static DEVICE_ATTR(irq, 0600 | 0200, irq_get, irq_ack);

/**
 * sysf node to check the interrupt status of the sensor, the interrupt
 * handler should perform sysf_notify to allow userland to poll the node.
 */
static ssize_t simulate_irq_get(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int irq = simulate_irq_value;
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);

	dev_info(fpc1020->dev, "%s irq = %d\n", __func__, irq);
	clear_bit(0, &simulate_irq_value);

	return scnprintf(buf, PAGE_SIZE, "%i\n", irq);
}

/*
 * writing to the irq node will just drop a printk message
 * and return success, used for latency measurement.
 */
static ssize_t simulate_irq_set(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	dev_info(fpc1020->dev, "%s, buf = %s\n", __func__, buf);

	set_bit(0, &simulate_irq_value);
	fpc1020_simulate_irq(dev);

	return count;
}
static DEVICE_ATTR(simulate_irq, S_IRUSR | S_IWUSR, simulate_irq_get, simulate_irq_set);

#ifdef CONFIG_FPC_COMPAT
static ssize_t compatible_all_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	int i;
	int irqf;
	struct  fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	dev_err(dev, "compatible all enter %d\n", fpc1020->compatible_enabled);
	if (!strncmp (buf, "enable", strlen ("enable")) && fpc1020->compatible_enabled != 1) {
		rc = fpc1020_request_named_gpio(fpc1020, "fpc,gpio_irq",
				&fpc1020->irq_gpio);
		if (rc)
			goto exit;

		rc = fpc1020_request_named_gpio(fpc1020, "fpc,gpio_rst",
				&fpc1020->rst_gpio);
		dev_err(dev, "fpc request reset result = %d\n", rc);
		if (rc)
			goto exit;
		fpc1020->fingerprint_pinctrl = devm_pinctrl_get(dev);
		if (IS_ERR(fpc1020->fingerprint_pinctrl)) {
			if (PTR_ERR(fpc1020->fingerprint_pinctrl) == -EPROBE_DEFER) {
				dev_info(dev, "pinctrl not ready\n");
				rc = -EPROBE_DEFER;
				goto exit;
			}
			dev_err(dev, "Target does not use pinctrl\n");
			fpc1020->fingerprint_pinctrl = NULL;
			rc = -EINVAL;
			goto exit;
		}

		for (i = 0; i < ARRAY_SIZE(fpc1020->pinctrl_state); i++) {
			const char *n = pctl_names[i];
			struct pinctrl_state *state =
				pinctrl_lookup_state(fpc1020->fingerprint_pinctrl, n);
			if (IS_ERR(state)) {
				dev_err(dev, "cannot find '%s'\n", n);
				rc = -EINVAL;
				goto exit;
			}
			dev_info(dev, "found pin control %s\n", n);
			fpc1020->pinctrl_state[i] = state;
		}
		rc = select_pin_ctl(fpc1020, "fpc1020_reset_reset");
		if (rc)
			goto exit;
		rc = select_pin_ctl(fpc1020, "fpc1020_irq_active");
		if (rc)
			goto exit;
		irqf = IRQF_TRIGGER_RISING | IRQF_ONESHOT;
		if (of_property_read_bool(dev->of_node, "fpc,enable-wakeup")) {
			irqf |= IRQF_NO_SUSPEND;
			device_init_wakeup(dev, 1);
		}
		rc = devm_request_threaded_irq(dev, gpio_to_irq(fpc1020->irq_gpio),
				NULL, fpc1020_irq_handler, irqf,
				dev_name(dev), fpc1020);
		if (rc) {
			dev_err(dev, "could not request irq %d\n",
					gpio_to_irq(fpc1020->irq_gpio));
			goto exit;
		}
		dev_dbg(dev, "requested irq %d\n", gpio_to_irq(fpc1020->irq_gpio));

		/* Request that the interrupt should be wakeable */
		enable_irq_wake(gpio_to_irq(fpc1020->irq_gpio));
		fpc1020->compatible_enabled = 1;
		if (of_property_read_bool(dev->of_node, "fpc,enable-on-boot")) {
			dev_info(dev, "Enabling hardware\n");
			(void)device_prepare(fpc1020, true);
		}
	} else if (!strncmp (buf, "disable", strlen ("disable")) && fpc1020->compatible_enabled != 0) {
		if (gpio_is_valid(fpc1020->irq_gpio)) {
			devm_gpio_free(dev, fpc1020->irq_gpio);
			dev_info(dev, "remove irq_gpio success\n");
		}
		if (gpio_is_valid(fpc1020->rst_gpio)) {
			devm_gpio_free(dev, fpc1020->rst_gpio);
			dev_info(dev, "remove rst_gpio success\n");
		}
		devm_free_irq(dev, gpio_to_irq(fpc1020->irq_gpio), fpc1020);
		fpc1020->compatible_enabled = 0;
	}
	hw_reset(fpc1020);
	return count;
exit:
	return -EINVAL;
}
static DEVICE_ATTR(compatible_all, S_IWUSR, NULL, compatible_all_set);
#endif

static ssize_t hotzone_value_get(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "hotzone: wakeup_enable:%d,left:%d,right:%d,top:%d,bottom:%d,update:%d\n",
			atomic_read(&fpc_data_bak->wakeup_enabled),
			hotzone_setting.left,
			hotzone_setting.right,
			hotzone_setting.top,
			hotzone_setting.bottom,
			hotzone_setting.update);
}

static ssize_t hotzone_value_set(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct fpc1020_data *fpc = dev_get_drvdata(dev);

	memcpy(&hotzone_setting, (uint8_t *)buf, count);
	dev_info(fpc->dev, "%s, hotzone:left:%d, right:%d, top:%d, bottom:%d, update:%d\r\n", __func__,
			hotzone_setting.left,
			hotzone_setting.right,
			hotzone_setting.top,
			hotzone_setting.bottom,
			hotzone_setting.update);
	return count;
}
static DEVICE_ATTR(hotzone_config, S_IRUSR | S_IWUSR, hotzone_value_get, hotzone_value_set);

/* finger_irq_flag
 * this flag will indicate the finger not in the hotzone
 * 0 --finger not int hotzone( finger_up)  (as the default value)
 * 1--finger in the hotzone (finger_down)
 * the return value, following with Hal define
 */

void fpc_hotzone_finger_irq_set(uint8_t flag)
{
	finger_irq_value = flag;
}

uint8_t fpc_hotzone_finger_irq_get(void)
{
	//printk("%s, finger_irq_flag:%d", __func__, finger_irq_flag);
	return finger_irq_value;
}

static ssize_t fpc_finger_irq_status_get(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	uint8_t status = fpc_hotzone_finger_irq_get();
	dev_info(dev, "%s status= %d\r\n", __func__, status);

	return scnprintf(buf, PAGE_SIZE, "%i\n", status);

}
static DEVICE_ATTR(finger_irq, S_IRUSR, fpc_finger_irq_status_get, NULL);

/*report finger detect irq to fingerprints HAL layer*/
int fpc_finger_irq_handler(uint8_t value)
{
	printk("%s   fingerprints hotzone finger irq\r\n", __func__);
	fpc_hotzone_finger_irq_set(value);
	sysfs_notify(&fpc_data_bak->dev->kobj, NULL, "finger_irq");
	return 0;

}
EXPORT_SYMBOL(fpc_finger_irq_handler);

/*this function will detect the touch position whether in the hotzone
*/
int fpc_hotzone_in(int x, int y, int p, int major)
{
	/*first check the wakeup irq status,
	 * if true, finger detect irq to fingerprints HAL layer
	 */
	if (atomic_read(&fpc_data_bak->wakeup_enabled) != true) {
		//printk("%s wakeup disabled\r\n", __func__);
		return 0;
	}

	/*judge the touch event*/
	if ((x >= TOUCHSCREEN_BOUNDARY_LOW_X) && (x <= TOUCHSCREEN_BOUNDARY_HIGH_X) &&
			(y >= TOUCHSCREEN_BOUNDARY_LOW_Y) && (y <= TOUCHSCREEN_BOUNDARY_HIGH_Y) &&
			(p >= TOUCHSCREEN_PRESSURE_THRESH) && (major >= TOUCHSCREEN_AREA_THRESH)) {
		/*ignore the pressue and area, some touch screen not support*/
		//if (p >= TOUCHSCREEN_PRESSURE_THRESH && major >= TOUCHSCREEN_AREA_THRESH)
		fpc_finger_irq_handler(HOTZONE_IN);
		///printk("%s shot the hotzone\r\n", __func__);
		return 1;
	}

	//printk("%s miss the hotzone\r\n", __func__);
	return 0;
}
EXPORT_SYMBOL(fpc_hotzone_in);

int fpc_hotzone_out(void)
{
	fpc_finger_irq_handler(HOTZONE_OUT);
	return 0;
}
EXPORT_SYMBOL(fpc_hotzone_out);


/*
 * this function used for report the hotzone touch event
 * @paramter input:
 * 1--finger  on in the hotzone
 * 0--finger up form the hotzone
 * this function called in the touch driver when report hotzone event
 * (with xiaomi design, when send event.keycode=0x152, call this function
 * the status = key_value(1 or 0))
 */
int fpc_hotzone_event_report(int status)
{
	if (status == 1) {
		/*
		 * only when the sensor wake up (that's means need capture images form sensor)
		 * should reprot finger down event.
		 */
		if (atomic_read(&fpc_data_bak->wakeup_enabled) != true) {
			return 0;
		}

		fpc_finger_irq_handler(HOTZONE_IN);
	} else {
		fpc_finger_irq_handler(HOTZONE_OUT);
	}
	return 0;
}
EXPORT_SYMBOL(fpc_hotzone_event_report);
static struct attribute *attributes[] = {
	&dev_attr_request_vreg.attr,
	&dev_attr_pinctl_set.attr,
	&dev_attr_device_prepare.attr,
	&dev_attr_regulator_enable.attr,
	&dev_attr_hw_reset.attr,
	&dev_attr_wakeup_enable.attr,
	&dev_attr_handle_wakelock.attr,
	&dev_attr_clk_enable.attr,
	&dev_attr_irq.attr,
	&dev_attr_simulate_irq.attr,
#ifdef CONFIG_FPC_COMPAT
	&dev_attr_compatible_all.attr,
#endif
	&dev_attr_finger_irq.attr,
	&dev_attr_hotzone_config.attr,
	NULL
};

static const struct attribute_group attribute_group = {
	.attrs = attributes,
};

static irqreturn_t fpc1020_simulate_irq(struct device *dev)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);

	dev_info(dev, "%s\n", __func__);
	sysfs_notify(&fpc1020->dev->kobj, NULL, dev_attr_simulate_irq.attr.name);

	return IRQ_HANDLED;
}
#ifdef CONFIG_FPC_COMPAT
static irqreturn_t fpc1020_irq_handler(int irq, void *handle)
{
	struct fpc1020_data *fpc1020 = handle;

	dev_dbg(fpc1020->dev, "%s\n", __func__);
	mutex_lock(&fpc1020->lock);
	if (atomic_read(&fpc1020->wakeup_enabled)) {
		fpc1020->nbr_irqs_received++;
		__pm_wakeup_event(&fpc1020->ttw_wl,
				msecs_to_jiffies(FPC_TTW_HOLD_TIME));
	}
	mutex_unlock(&fpc1020->lock);

	sysfs_notify(&fpc1020->dev->kobj, NULL, dev_attr_irq.attr.name);

	return IRQ_HANDLED;
}
#endif
static int fpc1020_request_named_gpio(struct fpc1020_data *fpc1020,
		const char *label, int *gpio)
{
	struct device *dev = fpc1020->dev;
	struct device_node *np = dev->of_node;
	int rc;

	rc = of_get_named_gpio(np, label, 0);

	if (rc < 0) {
		dev_err(dev, "failed to get '%s'\n", label);
		return rc;
	}
	*gpio = rc;

	rc = devm_gpio_request(dev, *gpio, label);
	if (rc) {
		dev_err(dev, "failed to request gpio %d\n", *gpio);
		return rc;
	}
	dev_dbg(dev, "%s %d\n", label, *gpio);

	return 0;
}

static int fpc1020_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int rc = 0;
#ifndef CONFIG_FPC_COMPAT
	size_t i;
	int irqf;
#endif

	struct device_node *np = dev->of_node;
	struct fpc1020_data *fpc1020 = devm_kzalloc(dev, sizeof(*fpc1020),
			GFP_KERNEL);

	dev_info(dev, "fpc probe start! \n");

	if (!fpc1020) {
		dev_err(dev,
				"failed to allocate memory for struct fpc1020_data\n");
		rc = -ENOMEM;
		goto exit;
	}

	fpc1020->dev = dev;
	fpc1020->vdd1v2_gpio = -1;
	fpc1020->vdd1v8_gpio = -1;
	fpc1020->vdd2v8_gpio = -1;
	fpc1020->gpios_requested = false;
	platform_set_drvdata(pdev, fpc1020);

	if (!np) {
		dev_err(dev, "fpc %s: no of node found\n", __func__);
		rc = -EINVAL;
		goto exit;
	}
#ifndef CONFIG_FPC_COMPAT

	fpc1020->fingerprint_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(fpc1020->fingerprint_pinctrl)) {
		if (PTR_ERR(fpc1020->fingerprint_pinctrl) == -EPROBE_DEFER) {
			dev_info(dev, "pinctrl not ready\n");
			rc = -EPROBE_DEFER;
			goto exit;
		}
		dev_err(dev, "Target does not use pinctrl\n");
		fpc1020->fingerprint_pinctrl = NULL;
		rc = -EINVAL;
		goto exit;
	}

	for (i = 0; i < ARRAY_SIZE(pctl_names); i++) {
		const char *n = pctl_names[i];
		struct pinctrl_state *state =
			pinctrl_lookup_state(fpc1020->fingerprint_pinctrl, n);
		if (IS_ERR(state)) {
			dev_err(dev, "cannot find '%s'\n", n);
			rc = -EINVAL;
			goto exit;
		}
		dev_info(dev, "found pin control %s\n", n);
		fpc1020->pinctrl_state[i] = state;
	}


	atomic_set(&fpc1020->wakeup_enabled, 0);

	irqf = IRQF_TRIGGER_RISING | IRQF_ONESHOT;
	if (of_property_read_bool(dev->of_node, "fpc,enable-wakeup")) {
		irqf |= IRQF_NO_SUSPEND;
		device_init_wakeup(dev, 1);
	}

	mutex_init(&fpc1020->lock);

	rc = sysfs_create_group(&dev->kobj, &attribute_group);
	if (rc) {
		dev_err(dev, "fpc %s: could not create sysfs!\n", __func__);
		goto exit;
	}

	if (of_property_read_bool(dev->of_node, "fpc,enable-on-boot")) {
		dev_info(dev, "fpc Enabling hardware\n");
		(void)device_prepare(fpc1020, true);

		fpc_data_bak = fpc1020;
		rc = hw_reset(fpc1020);
		if (rc) {
			dev_err(dev, "fpc hardware reset failed\n");
			goto exit;
		}
	}
#else
	mutex_init(&fpc1020->lock);
	wake_lock_init(&fpc1020->ttw_wl, WAKE_LOCK_SUSPEND, "fpc_ttw_wl");

	rc = sysfs_create_group(&dev->kobj, &attribute_group);
	if (rc) {
		dev_err(dev, "could not create sysfs\n");
		goto exit;
	}

	mutex_lock(&fpc1020->lock);
	/*initial power control gpio pin */
	rc = select_pin_ctl(fpc1020, "fpc1020_vdd1v2_default");
	if (rc) {
		dev_err(dev, "fpc initial power 1v2 gpio failed!\n");
		goto exit;
	}

	rc = select_pin_ctl(fpc1020, "fpc1020_vdd1v8_default");
	if (rc) {
		dev_err(dev, "fpc initial power 1v8 gpio failed!\n");
		goto exit;
	}

	mutex_unlock(&fpc1020->lock);

#endif
	dev_info(dev, "fpc %s: ok\n", __func__);
	return rc;

exit:
	dev_err(dev, "fpc %s: failed!\n", __func__);
	return rc;
}

static int fpc1020_remove(struct platform_device *pdev)
{
	struct fpc1020_data *fpc1020 = platform_get_drvdata(pdev);

	sysfs_remove_group(&pdev->dev.kobj, &attribute_group);
	mutex_destroy(&fpc1020->lock);
	wakeup_source_trash(&fpc1020->ttw_wl);
	(void)vreg_setup(fpc1020, "vdd_ana", false);
	(void)vreg_setup(fpc1020, "vdd_io", false);
	(void)vreg_setup(fpc1020, "vdd_spi", false);
	dev_info(&pdev->dev, "%s\n", __func__);

	return 0;
}

static struct of_device_id fpc1020_of_match[] = {
	{ .compatible = "fpc,fpc1020", },
	{}
};
MODULE_DEVICE_TABLE(of, fpc1020_of_match);

static struct platform_driver fpc1020_driver = {
	.driver = {
		.name	= "fpc1020",
		.owner	= THIS_MODULE,
		.of_match_table = fpc1020_of_match,
	},
	.probe	= fpc1020_probe,
	.remove	= fpc1020_remove,
};

static int __init fpc1020_init(void)
{
	int rc = platform_driver_register(&fpc1020_driver);

	if (!rc)
		pr_info("%s OK\n", __func__);
	else
		pr_err("%s %d\n", __func__, rc);

	return rc;
}

static void __exit fpc1020_exit(void)
{
	pr_info("%s\n", __func__);
	platform_driver_unregister(&fpc1020_driver);
}

module_init(fpc1020_init);
module_exit(fpc1020_exit);


MODULE_DESCRIPTION("FPC1020 Fingerprint sensor device driver.");
MODULE_LICENSE("GPL v2");
