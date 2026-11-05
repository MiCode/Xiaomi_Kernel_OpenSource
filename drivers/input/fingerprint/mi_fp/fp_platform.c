#include "fp_driver.h"
#include "../../../misc/hwid/hwid.h"

/* -------------------------------------------------------------------- */
/* fingerprint chip hardware configuration				*/
/* -------------------------------------------------------------------- */
#define DTS_NETLINK_NUM             "netlink-event"
#define SENSORLOCATION              "sensor-loc"
#define DTS_RST_HIGH_TIME           "xiaomi,rst_high_time"
#define DTS_RST_LOW_TIME            "xiaomi,rst_low_time"
#define DTS_IRQ_GPIO                "xiaomi,gpio_irq"
#define DTS_VENDOR_NAME             "xiaomi,vendor_names"
#define DTS_PINCTL_RESET_HIGH       "reset_high"
#define DTS_PINCTL_RESET_LOW        "reset_low"
#ifndef FP_ULTRA_QCOM_P11
#define DTS_RST_PULL_FLAG           "reset-flag"
#else
#define DTS_PINCTL_IRQ_DEFAULT      "irq_default"
#define DTS_PINCTL_IRQ_PULLDOWN     "irq_pulldown"
#define DTS_RESET_PIN_ENABLE_FLAG   "reset-pin-enable"
#define DTS_RESET_PULL_FLAG         "reset-flag"
#define DTS_INTR2_ENABLE_FLAG       "intr2-enable"
#define DTS_INTR2_SIDE_CONFIG       "intr2-side-config"
#endif

#define HW_LEVEL_P01                 1
#define HW_LEVEL_MASK               0XFF0F000F

int fp_parse_dts(struct fp_device *fp_dev)
{
	int ret;
        uint32_t platform_id, hw_level;
	struct device_node *node = fp_dev->driver_device->dev.of_node;
	FUNC_ENTRY();
	if (node) {
#ifdef FP_ULTRA_QCOM_P11
		/* get fingerprint pinctrl config */
		fp_dev->pinctrl = devm_pinctrl_get(&fp_dev->driver_device->dev);
		if (IS_ERR(fp_dev->pinctrl)) {
			ret = PTR_ERR(fp_dev->pinctrl);
			pr_debug("can't find fingerprint pinctrl\n");
		}
#endif

		/* get irq resource */
		fp_dev->irq_gpio = of_get_named_gpio(node, DTS_IRQ_GPIO, 0);
		pr_debug("fp::irq_gpio:%d\n", fp_dev->irq_gpio);
		if (!gpio_is_valid(fp_dev->irq_gpio)) {
#ifdef FP_ULTRA_QCOM_P11
			pr_err("IRQ GPIO is invalid.\n");
#else
			pr_debug("IRQ GPIO is invalid.\n");
#endif
			return -EPERM;
		}
		fp_dev->irq_num = gpio_to_irq(fp_dev->irq_gpio);

#ifdef FP_ULTRA_QCOM_P11
		if (fp_dev->pinctrl) {
			fp_dev->pins_int_default =
				pinctrl_lookup_state(fp_dev->pinctrl, DTS_PINCTL_IRQ_DEFAULT);
			if (IS_ERR(fp_dev->pins_int_default)) {
				ret = PTR_ERR(fp_dev->pins_int_default);
				pr_debug("can't find pinctrl irq_default\n");
			}

			fp_dev->pins_int_pulldown =
				pinctrl_lookup_state(fp_dev->pinctrl, DTS_PINCTL_IRQ_PULLDOWN);
			if (IS_ERR(fp_dev->pins_int_pulldown)) {
				ret = PTR_ERR(fp_dev->pins_int_pulldown);
				pr_debug("can't find pinctrl irq_pulldown");
			}
		}
#else
		/* get reset pin info */
		ret = of_property_read_u32(node, DTS_RST_HIGH_TIME, &fp_dev->rst_high_delay);
		if (ret != 0 || fp_dev->rst_high_delay == 0) {
			fp_dev->rst_high_delay = FP_RESET_DEFAULT_TIME;
		}
		ret = of_property_read_u32(node, DTS_RST_LOW_TIME, &fp_dev->rst_low_delay);
		if (ret != 0 || fp_dev->rst_low_delay == 0) {
			fp_dev->rst_low_delay = FP_RESET_DEFAULT_TIME;
		}
		pr_debug("rst_high_delay = %u, rst_low_delay = %u\n", fp_dev->rst_high_delay, fp_dev->rst_low_delay);
#endif

		/* get netlink-event */
		of_property_read_u32(node, DTS_NETLINK_NUM, &fp_dev->fp_netlink_num);

		/* get fingerprint vendor info */
		of_property_read_string(node, DTS_VENDOR_NAME, &fp_dev->vendor_names);
		pr_debug("netlink_num = %d, vendor_names = %s\n", fp_dev->fp_netlink_num, fp_dev->vendor_names);

#ifdef FP_ULTRA_QCOM
#ifdef FP_ULTRA_QCOM_P11
		/* get intr2 enable status and get intr2 resource */
		if (of_property_read_bool(node, DTS_INTR2_ENABLE_FLAG)) {
			if (of_find_property(node, DTS_INTR2_SIDE_CONFIG, NULL)) {
				const char *str;
				of_property_read_string(node, DTS_INTR2_SIDE_CONFIG, &str);
				if (strcmp(str, "AP") == 0) {
					fp_dev->intr2_side_config = 1;
#ifdef FP_INTR2_AP_PER_LOW
					fp_dev->intr2_ctrl_enable = 1;
#endif	// FP_INTR2_AP_PER_LOW
					pr_debug("intr2 AP is enable.\n");
				} else if (strcmp(str, "TIC") == 0) {
					fp_dev->intr2_side_config = 2;
					pr_debug("intr2 TIC is enable.\n");
				} else {
					fp_dev->intr2_side_config = 0;
					pr_debug("intr2 is not config\n");
				}
			} else {
				pr_debug("INTR2 is not set.\n");
			}
		}else {
			pr_debug("INTR2 is not enable.\n");
		}
#endif

#ifdef FP_INTR2_AP_PER_LOW
		/* Intr2 is enable */
		if (fp_dev->intr2_side_config != 0)
		{
			/* Intr2 config to AP side */
			if (fp_dev->intr2_side_config == 1) {
				fp_dev->intr2_AP_pin_enable = true;
				/* Init Intr2 Pinctrl */
				if (fp_dev->pinctrl) {
					fp_dev->pins_reset_high =
						pinctrl_lookup_state(fp_dev->pinctrl, DTS_PINCTL_RESET_HIGH);
					if (IS_ERR(fp_dev->pins_reset_high)) {
						ret = PTR_ERR(fp_dev->pins_reset_high);
						pr_debug("can't find pinctrl intr2_high\n");
						fp_dev->intr2_AP_pin_enable = false;
					}

					fp_dev->pins_reset_low =
						pinctrl_lookup_state(fp_dev->pinctrl, DTS_PINCTL_RESET_LOW);
					if (IS_ERR(fp_dev->pins_reset_low)) {
						ret = PTR_ERR(fp_dev->pins_reset_low);
						pr_debug("can't find pinctrl reset_low\n");
						fp_dev->intr2_AP_pin_enable = false;
					} else {
						pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_low);
					}

					if (fp_dev->intr2_AP_pin_enable) {
						pr_debug("intr2 AP is enable.\n");
					}
				} else {
					fp_dev->intr2_AP_pin_enable = false;
					pr_debug("pinctrl is null\n");
				}
			}
			/* Intr2 config to TIC side */
			else if (fp_dev->intr2_side_config == 2) {
				pr_debug("intr2 TIC is enable.\n");
			}
		}
#endif

		/* get ipc resourece */
		fp_dev->fw_ipc.gpio = of_get_named_gpio(node, DTS_IPC_GPIO, 0);
		pr_debug("fp::ipc_gpio:%d\n", fp_dev->fw_ipc.gpio);
		if (!gpio_is_valid(fp_dev->fw_ipc.gpio)) {
			pr_debug("IPC GPIO is invalid.\n");
			return -EPERM;
		}
		fp_dev->fw_ipc.irq = gpio_to_irq(fp_dev->fw_ipc.gpio);

#ifndef FP_ULTRA_QCOM_P11
		/* get intr2 resourece*/
		fp_dev->intr2_gpio = of_get_named_gpio(node, DTS_INTR2_GPIO, 0);
		if (!gpio_is_valid(fp_dev->intr2_gpio))
			pr_debug("intr2 gpio not found, gpio=%d\n", fp_dev->intr2_gpio);

		of_property_read_u32_array(node, SENSORLOCATION, fp_dev->position.location, 4);
		/*get wuhb resourece */
		/*fp_dev->fd_gpio.gpio = of_get_named_gpio(node, DTS_WUHB_GPIO, 0);
		pr_debug( "fp::fd_gpio:%d\n", fp_dev->fd_gpio.gpio);
		if (!gpio_is_valid(fp_dev->fd_gpio.gpio)) {
			fp_dev->is_wuhb_connected = 0;
			pr_debug("FD GPIO is invalid.\n");
			return -EPERM;
		}
		fp_dev->is_wuhb_connected = 1;
		fp_dev->fd_gpio.active_low = FD_GPIO_ACTIVE_LOW;
		pr_debug("is_wuhb_connected=%d\n", fp_dev->is_wuhb_connected);*/

		atomic_set(&fp_dev->fd_available, 1);
#endif
		atomic_set(&fp_dev->ipc_available, 1);
		atomic_set(&fp_dev->wakelock_acquired, 0);

		mutex_init(&fp_dev->mutex);
#ifndef FP_ULTRA_QCOM_P11
		mutex_init(&fp_dev->fd_events_mutex);
#endif
		mutex_init(&fp_dev->ipc_events_mutex);
#ifndef FP_ULTRA_QCOM_P11
		fp_dev->fd_gpio.work_init = false;
#endif
		fp_dev->fw_ipc.work_init = false;
#endif

		platform_id = get_hw_version_platform();

		hw_level = (((get_hw_id_value() & HW_LEVEL_MASK) >> 16) & 0xF) * 10
  			+ (((get_hw_id_value() & HW_LEVEL_MASK) >> 0) & 0xF);

		if((platform_id == HARDWARE_PROJECT_P11)&&(hw_level ==  HW_LEVEL_P01)) {
			fp_dev->vreg_3v3.mRegulator = regulator_get(&fp_dev->driver_device->dev, DTS_VOlT_REGULATER_3V1);
			pr_debug(" p11 fp voltage is P01 L9B\n");
		} else{
			fp_dev->vreg_3v3.mRegulator = regulator_get(&fp_dev->driver_device->dev, DTS_VOlT_REGULATER_3V3);
	        }
		fp_dev->vreg_1v8.mRegulator = regulator_get(&fp_dev->driver_device->dev, DTS_VOlT_REGULATER_1V8);
		fp_dev->vreg_gpio.mPwrGpio = of_get_named_gpio(node, DTS_VOlT_REGULATER_GPIO, 0);
	} else {
		pr_debug("device node is null\n");
			return -EPERM;
	}

#ifdef FP_ULTRA_QCOM_P11
	/* get reset pin enable status and get reset pin resource */
	if (of_property_read_bool(node, DTS_RESET_PIN_ENABLE_FLAG)) {
		/* get whether the rst pin needs the flag bit of pull */
		of_property_read_u32_array(node, DTS_RESET_PULL_FLAG, fp_dev->rst_pull_flag, 5);
		pr_debug("rst_pull_flag = %d %d %d %d %d\n",
			fp_dev->rst_pull_flag[0], fp_dev->rst_pull_flag[1], fp_dev->rst_pull_flag[2],
			fp_dev->rst_pull_flag[3], fp_dev->rst_pull_flag[4]);

		/* get reset pin info */
		ret = of_property_read_u32(node, DTS_RST_HIGH_TIME, &fp_dev->rst_high_delay);
		if (ret != 0 || fp_dev->rst_high_delay == 0) {
			fp_dev->rst_high_delay = FP_RESET_DEFAULT_TIME;
		}
		ret = of_property_read_u32(node, DTS_RST_LOW_TIME, &fp_dev->rst_low_delay);
		if (ret != 0 || fp_dev->rst_low_delay == 0) {
			fp_dev->rst_low_delay = FP_RESET_DEFAULT_TIME;
		}

		pr_debug("reset-pin is enable.\n");
		fp_dev->reset_pin_enable = true;

		if (fp_dev->pinctrl) {
			fp_dev->pins_reset_high =
				pinctrl_lookup_state(fp_dev->pinctrl, DTS_PINCTL_RESET_HIGH);
			if (IS_ERR(fp_dev->pins_reset_high)) {
				ret = PTR_ERR(fp_dev->pins_reset_high);
				pr_debug("can't find pinctrl reset_high\n");
				fp_dev->reset_pin_enable = false;
			}

			fp_dev->pins_reset_low =
				pinctrl_lookup_state(fp_dev->pinctrl, DTS_PINCTL_RESET_LOW);
			if (IS_ERR(fp_dev->pins_reset_low)) {
				ret = PTR_ERR(fp_dev->pins_reset_low);
				pr_debug("can't find pinctrl reset_low\n");
				fp_dev->reset_pin_enable = false;
			} else {
				pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_low);
			}
		}
	} else {
		pr_debug("reset-pin is not enable.\n");
	}

	pr_debug("irq_num = %d, rst_high_delay = %u, rst_low_delay = %u\n",
		fp_dev->irq_num, fp_dev->rst_high_delay, fp_dev->rst_low_delay);
#else
	if (fp_dev->driver_device) {

		fp_dev->pinctrl = devm_pinctrl_get(&fp_dev->driver_device->dev);
		if (IS_ERR(fp_dev->pinctrl)) {
			ret = PTR_ERR(fp_dev->pinctrl);
			pr_debug("can't find fingerprint pinctrl\n");
			return ret;
		}

		fp_dev->pins_reset_high =
			pinctrl_lookup_state(fp_dev->pinctrl, DTS_PINCTL_RESET_HIGH);
		if (IS_ERR(fp_dev->pins_reset_high)) {
			ret = PTR_ERR(fp_dev->pins_reset_high);
			pr_debug("can't find  pinctrl reset_high\n");
#ifndef FP_ULTRA_QCOM_P11
			return ret;
#endif
		}

		fp_dev->pins_reset_low =
			pinctrl_lookup_state(fp_dev->pinctrl, DTS_PINCTL_RESET_LOW);
		if (IS_ERR(fp_dev->pins_reset_low)) {
			ret = PTR_ERR(fp_dev->pins_reset_low);
			pr_debug("can't find  pinctrl reset_low\n");
#ifndef FP_ULTRA_QCOM_P11
			return ret;
#endif
		} else {
			pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_low);
		}

		pr_debug("get pinctrl success!\n");
	} else {
		pr_debug("platform device is null\n");
		return -EPERM;
	}
#endif

	FUNC_EXIT();
	return 0;
}

void fp_power_config(struct fp_device *fp_dev)
{
	fp_dev->vreg_3v3.enable = false;
	fp_dev->vreg_1v8.enable = false;
	fp_dev->vreg_gpio.enable = false;

	fp_dev->vreg_3v3.IsGpio = false;
	fp_dev->vreg_1v8.IsGpio = false;
	fp_dev->vreg_gpio.IsGpio = true;

	fp_dev->vreg_3v3.mRegulator = NULL;
	fp_dev->vreg_1v8.mRegulator = NULL;
	fp_dev->vreg_gpio.mPwrGpio = -EINVAL;
}

int fp_power_on(struct fp_vreg *vreg)
{
	int status = 0;
	int retval = 0;

	uint32_t hw_level;

	FUNC_ENTRY();
	if (!vreg->IsGpio) {
		if (vreg->mRegulator != NULL) {
			if (!vreg->enable) {
#ifdef CONFIG_FP_MTK_PLATFORM
				regulator_set_voltage(vreg->mRegulator, 3300000, 3300000);
#endif

	hw_level = (((get_hw_id_value() & HW_LEVEL_MASK) >> 16) & 0xF) * 10
                       + (((get_hw_id_value() & HW_LEVEL_MASK) >> 0) & 0xF);
	pr_debug("hw_level is  %d!!\n", hw_level);
#ifdef FP_ULTRA_QCOM_P11
	status = regulator_enable(vreg->mRegulator);
	mdelay(1);
	status = regulator_get_voltage(vreg->mRegulator);
	pr_debug("power on Ldo regulator value is %d!!\n", status);
	if(status == 3100000){
		regulator_set_voltage(vreg->mRegulator, 3300000, 3300000);
		mdelay(1);
		status = regulator_enable(vreg->mRegulator);
	 	mdelay(1);
		status = regulator_get_voltage(vreg->mRegulator);
	}
#else
				status = regulator_enable(vreg->mRegulator);
				mdelay(1);
				status = regulator_get_voltage(vreg->mRegulator);
#endif
				vreg->enable = true;
				pr_debug("power on Ldo regulator value is %d!!\n", status);
			} else {
				pr_err("mRegulator is already power-on.");
			}
		} else {
			pr_err("mRegulator is NULL.");
			retval = -ENXIO;
		}
	} else {
		if (vreg->mPwrGpio != -EINVAL) {
			if (!vreg->enable) {
				if (!gpio_is_valid(vreg->mPwrGpio)) {
					pr_debug("mPwrGpio is invalid.\n");
					retval = -EINVAL;
				} else {
					gpio_direction_output(vreg->mPwrGpio, 0);
					mdelay(1);
					gpio_direction_output(vreg->mPwrGpio, 1);
					mdelay(1);

					vreg->enable = true;
					pr_debug("mPwrGpio power-on success.\n");
				}
			} else {
				pr_err("mPwrGpio is already power-on.");
			}
		} else {
			pr_err("mPwrGpio is NULL.");
			retval = -ENXIO;
		}
	}

	return retval;
}

int fp_power_off(struct fp_vreg *vreg)
{
	int status = 0;
	int retval = 0;

	FUNC_ENTRY();

	if (!vreg->IsGpio) {
		if (vreg->mRegulator != NULL) {
			if (vreg->enable) {
				status = regulator_disable(vreg->mRegulator);
				mdelay(1);
				status = regulator_get_voltage(vreg->mRegulator);
				vreg->enable = false;
				pr_debug("power off Ldo regulator_value %d!!\n", status);
			} else {
				pr_err("mRegulator is already power-off.");
			}
		} else {
			pr_err("mRegulator is NULL.");
			retval = -ENXIO;
		}
	} else {
		if (vreg->mPwrGpio != -EINVAL) {
			if (vreg->enable) {
				if (!gpio_is_valid(vreg->mPwrGpio)) {
					pr_debug("mPwrGpio is invalid.\n");
					retval = -EINVAL;
				} else {
					gpio_direction_output(vreg->mPwrGpio, 0);
					mdelay(1);
					vreg->enable = false;
					pr_debug("mPwrGpio power-off success.\n");
				}
			} else {
				pr_err("mPwrGpio is already power-off.");
			}
		} else {
			pr_err("mPwrGpio is NULL.");
			retval = -ENXIO;
		}
	}

	return retval;
}

void fp_hw_reset(struct fp_device *fp_dev)
{
	pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_low);
	mdelay(fp_dev->rst_low_delay);
	pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_high);
	mdelay(fp_dev->rst_high_delay);
}

/* Delay adjustable time before and after reset */
void fp_hw_reset_delay(struct fp_device *fp_dev, u_int32_t rst_low_delay, u_int32_t rst_high_delay)
{
	pr_debug( "rst_low time = %u, rst_high time = %u.\n", rst_low_delay, rst_high_delay);
	pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_low);
	mdelay(rst_low_delay);
	pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_high);
	mdelay(rst_high_delay);
}
void fp_acquire_stay_wake_time_ms(struct fp_device *fp_dev, unsigned int msec)
{
	if (fp_dev->wakesrc == NULL) {
		pr_err("fp wakeup source time is NULL\n");
		return;
	}
	__pm_wakeup_event(fp_dev->wakesrc, msec);
}

void fp_acquire_stay_wake(struct fp_device *fp_dev)
{
	if (fp_dev->wakesrc == NULL) {
	pr_err("fp wakeup source is NULL\n");
	return;
	}
	__pm_stay_awake(fp_dev->wakesrc);
}

void fp_release_stay_wake(struct fp_device *fp_dev)
{
	if (fp_dev->wakesrc == NULL) {
	pr_err("fp wakeup source is NULL\n");
	return;
	}
	__pm_relax(fp_dev->wakesrc);
}
void fp_enable_irq(struct fp_device *fp_dev)
{
	if (1 == fp_dev->irq_enabled) {
		pr_debug( "irq already enabled\n");
	} else {
		enable_irq(fp_dev->irq_num);
		fp_dev->irq_enabled = 1;
		pr_debug( "enable irq!\n");
	}
}

void fp_disable_irq(struct fp_device *fp_dev)
{
	if (0 == fp_dev->irq_enabled) {
		pr_debug( "irq already disabled\n");
	} else {
		disable_irq(fp_dev->irq_num);
		fp_dev->irq_enabled = 0;
		pr_debug("disable irq!\n");
	}
}

void fp_kernel_key_input(struct fp_device *fp_dev, struct fp_key *fp_key)
{
	uint32_t key_input = 0;

	if (FP_KEY_HOME == fp_key->key) {
		key_input = FP_KEY_INPUT_HOME;
	} else if (FP_KEY_HOME_DOUBLE_CLICK == fp_key->key) {
                key_input = FP_KEY_DOUBLE_CLICK;
        } else if (FP_KEY_POWER == fp_key->key) {
		key_input = FP_KEY_INPUT_POWER;
	} else if (FP_KEY_CAMERA == fp_key->key) {
		key_input = FP_KEY_INPUT_CAMERA;
	} else {
		/* add special key define */
		key_input = fp_key->key;
	}

	pr_debug("received key event[%d], key=%d, value=%d\n",
		 key_input, fp_key->key, fp_key->value);

	if ((FP_KEY_POWER == fp_key->key || FP_KEY_CAMERA == fp_key->key)
		&& (fp_key->value == 1)) {
		input_report_key(fp_dev->input, key_input, 1);
		input_sync(fp_dev->input);
		input_report_key(fp_dev->input, key_input, 0);
		input_sync(fp_dev->input);
	}

	if (FP_KEY_HOME_DOUBLE_CLICK == fp_key->key) {
                pr_debug("input report key event double click");
		input_report_key(fp_dev->input, key_input, fp_key->value);
		input_sync(fp_dev->input);
	}
}

void fp_local_time_printk(const char *format, ...)
{
	struct timespec64 tv;
	struct rtc_time tm;
	unsigned long local_time;
	struct va_format vaf;
	va_list args;

	ktime_get_real_ts64(&tv);
	/* Convert rtc to local time */
	local_time = (u32)(tv.tv_sec - (sys_tz.tz_minuteswest * 60));
	rtc_time64_to_tm(local_time, &tm);

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	printk("xiaomi-fp [%d-%02d-%02d %02d:%02d:%02d.%06lu] %pV",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_nsec / 1000,
			&vaf);

	va_end(args);
}

