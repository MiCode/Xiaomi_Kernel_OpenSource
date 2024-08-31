#include "fp_driver.h"
/* -------------------------------------------------------------------- */
/* fingerprint chip hardware configuration				*/
/* -------------------------------------------------------------------- */
#define DTS_NETLINK_NUM				"netlink-event"
#define DTS_IRQ_GPIO				"xiaomi,gpio_irq"
#define DTS_PINCTL_RESET_HIGH			"reset_high"
#define DTS_PINCTL_RESET_LOW			"reset_low"

int fp_parse_dts(struct fp_device *fp_dev)
{
	int ret;
	struct device_node *node = fp_dev->driver_device->dev.of_node;
	FUNC_ENTRY();
	if (node) {
	/*get irq resourece */
		fp_dev->irq_gpio =of_get_named_gpio(node, DTS_IRQ_GPIO, 0);
		pr_debug( "fp::irq_gpio:%d\n",fp_dev->irq_gpio);
		if (!gpio_is_valid(fp_dev->irq_gpio)) {
			pr_debug("IRQ GPIO is invalid.\n");
			return -EPERM;
		}
		fp_dev->irq_num = gpio_to_irq(fp_dev->irq_gpio);
		of_property_read_u32(node, DTS_NETLINK_NUM, &fp_dev->fp_netlink_num);
	} else {
		pr_debug( "device node is null\n");
			return -EPERM;
	}
	if (fp_dev->driver_device) {
		fp_dev->pinctrl = pinctrl_get(&fp_dev->driver_device->dev);
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
			return ret;
		}
		fp_dev->pins_reset_low =
			pinctrl_lookup_state(fp_dev->pinctrl, DTS_PINCTL_RESET_LOW);
		if (IS_ERR(fp_dev->pins_reset_low)) {
			ret = PTR_ERR(fp_dev->pins_reset_low);
			pr_debug("can't find  pinctrl reset_low\n");
			return ret;
		} else {
			pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_low);
		}
		/*
		fp_dev->pins_spiio_spi_mode =
	    	pinctrl_lookup_state(fp_dev->pinctrl, "spiio_spi_mode");
		if (IS_ERR(fp_dev->pins_spiio_spi_mode)) {
			ret = PTR_ERR(fp_dev->pins_spiio_spi_mode);
			pr_debug("%s can't find fingerprint pinctrl spiio_spi_mode\n",
				__func__);
		} else {
			pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_spiio_spi_mode);
		}

		fp_dev->pins_spiio_gpio_mode =
	    	pinctrl_lookup_state(fp_dev->pinctrl, "spiio_gpio_mode");
		if (IS_ERR(fp_dev->pins_spiio_gpio_mode)) {
			ret = PTR_ERR(fp_dev->pins_spiio_gpio_mode);
			pr_debug("%s can't find fingerprint spiio_gpio_mode\n",
				__func__);
		}

		fp_dev->pins_eint_default =
			pinctrl_lookup_state(fp_dev->pinctrl, "eint_default");
		if (IS_ERR(fp_dev->pins_eint_default)) {
			ret = PTR_ERR(fp_dev->pins_eint_default);
			pr_debug("%s can't find fingerprint pinctrl pins_eint_default\n",
				__func__);
			return ret;
		}

		fp_dev->pins_eint_pulldown =
	    	pinctrl_lookup_state(fp_dev->pinctrl, "eint_pulldown");
		if (IS_ERR(fp_dev->pins_eint_pulldown)) {
			ret = PTR_ERR(fp_dev->pins_eint_pulldown);
			pr_debug("%s can't find fingerprint pinctrl pins_eint_pulldown\n",
				__func__);
			return ret;

		} else {
			pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_eint_pulldown);
		}
		*/
		pr_debug("get pinctrl success!\n");
	} else {
		pr_debug( "platform device is null\n");
		return -EPERM;
	}
	FUNC_EXIT();
	return 0;
}

void fp_power_on(struct fp_device *fp_dev)
{
	int status=0;
	FUNC_ENTRY();
	if(fp_dev->device_available == 1){
		pr_err("have already powered on\n");
	}else{
#ifdef CONFIG_FP_MTK_PLATFORM
		status = regulator_set_voltage(fp_dev->vreg, 3300000, 3300000);
#endif
		status = regulator_enable(fp_dev->vreg);
		status = regulator_get_voltage(fp_dev->vreg);
		pr_debug( "power on regulator_value %d!!\n",status);
		fp_dev->device_available = 1;
	}
}

void fp_power_off(struct fp_device *fp_dev)
{
	int status=0;
	FUNC_ENTRY();
	if(fp_dev->device_available == 0){
		pr_err("has already powered off\n");
	}else{
		status = regulator_disable(fp_dev->vreg);
		status = regulator_get_voltage(fp_dev->vreg);
		pr_debug( "power off regulator_value %d!!\n",status);
		fp_dev->device_available = 0;
	}
}

/* delay ms after reset */
void fp_hw_reset(struct fp_device *fp_dev, u8 delay_ms)
{
	pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_low);
	mdelay(10);
	pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_high);
	mdelay(delay_ms);
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

void fp_local_time_printk(const char *level, const char *format, ...)
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

