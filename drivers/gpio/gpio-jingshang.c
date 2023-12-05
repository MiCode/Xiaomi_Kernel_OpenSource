#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/pm_wakeirq.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
 #include <linux/regulator/consumer.h>


#define JINGSHANG_GPIO_COUNT 7

#define JINGSHANG_DEV_MODE_MSK (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH)	/*664*/

enum {
	JINGSHANG_GPIO_BP_SLEEP_AP = 0,
	JINGSHANG_GPIO_AP_SLEEP_BP,
	JINGSHANG_GPIO_RST,
	JINGSHANG_GPIO_SIM_CTL0,
	JINGSHANG_GPIO_SIM_CTL1,
	JINGSHANG_GPIO_SAT_1P8_EN,
	JINGSHANG_GPIO_TT_UART_EN,
};

enum {
	JINGSHANG_REG_POW_CTL_1P1 = 0,
	JINGSHANG_REG_POW_CTL_3P3
};


static const char * const jingshang_gpio_names[] = {
	"gpio-bq-sleep-ap",
	"gpio-aq-sleep-bp",
	"gpio-rst",
	"gpio-sim-ctl0",
	"gpio-sim-ctl1",
	"gpio-sat-1p8-en",
	"gpio-tt-uart-en",
};

struct gpio_data {
	int irq;
	int gpio_num;
	int gpio_status;
};

struct jingshang_gpio_data {
	struct device *dev;
	int debounce_time;
	int current_irq;

	struct mutex lock;	/* To set/get exported values in sysfs */

	struct delayed_work debounce_work;
	struct gpio_data *data;
	struct regulator *vdd_1p1;
	struct regulator *vdd_3p3;
};


static ssize_t gpio_jingshang_set_general(struct device *device, int gpio_type, const char *buf, size_t count)
{
	int rc = -1;
	int gpio = 0;
	struct jingshang_gpio_data *jingshang_data;

	jingshang_data = dev_get_drvdata(device);
	gpio = jingshang_data->data[gpio_type].gpio_num;

	mutex_lock(&jingshang_data->lock);
	if (!strncmp(buf, "1", strlen("1"))) {
		if (gpio_is_valid(gpio)) {
			rc = gpio_direction_output(gpio, 1);
			if (rc) {
				dev_err(device, "jingshang %s: fail to set gpio %d !\n", __func__, gpio);
				goto unlock_ret;
			}
		} else {
			dev_err(device, "jingshang %s: unable to get gpio %d!\n", __func__, gpio);
		}
	} 
	else if (!strncmp(buf, "0", strlen("0"))) {
		if (gpio_is_valid(gpio)) {
			rc = gpio_direction_output(gpio, 0);
			if (rc) {
				dev_err(device, "jingshang %s: fail to set gpio %d !\n", __func__, gpio);
				goto unlock_ret;
			}
		} else {
			dev_err(device, "jingshang %s: unable to get gpio %d!\n", __func__, gpio);
		}
	} 
	else 
	{
		rc = -EINVAL;
		dev_err(device, "jingshang %s: invalid input %s!only 0 or 1 is valid.\n", __func__, buf);
	}

unlock_ret:
	mutex_unlock(&jingshang_data->lock);
	dev_info(device, "%s, gpio_type [%d] set, rc [%d], buf = %s\n", __func__, gpio_type, rc, buf);
	return count;
}



static ssize_t gpio_jingshang_get_general(struct device *device, int gpio_type, char *buf)
{
	int value = -1;
	int gpio = 0;
	struct jingshang_gpio_data *jingshang_data;
	jingshang_data = dev_get_drvdata(device);

	mutex_lock(&jingshang_data->lock);
	jingshang_data = dev_get_drvdata(device);
	gpio = jingshang_data->data[gpio_type].gpio_num;

	if (gpio_is_valid(gpio)) {
		value = gpio_get_value(gpio);
	} else {
		dev_err(device, "jingshang %s: unable to get gpio %d!\n", __func__, gpio);
	}
	mutex_unlock(&jingshang_data->lock);

	dev_info(device, "%s, gpio_type [%d] get, value [%d]\n", __func__, gpio_type, value);
	return sysfs_emit(buf, "%d\n", value);
}



static ssize_t gpio_jingshang_get_bp_sleep_ap(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_jingshang_get_general(device, JINGSHANG_GPIO_BP_SLEEP_AP, buf);
}

static ssize_t gpio_jingshang_set_bp_sleep_ap(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_jingshang_set_general(device, JINGSHANG_GPIO_BP_SLEEP_AP, buf, count);
}

static DEVICE_ATTR(bp_sleep_ap, JINGSHANG_DEV_MODE_MSK, gpio_jingshang_get_bp_sleep_ap, gpio_jingshang_set_bp_sleep_ap);

static ssize_t gpio_jingshang_get_ap_sleep_bp(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_jingshang_get_general(device, JINGSHANG_GPIO_AP_SLEEP_BP, buf);
}

static ssize_t gpio_jingshang_set_ap_sleep_bp(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_jingshang_set_general(device, JINGSHANG_GPIO_AP_SLEEP_BP, buf, count);
}

static DEVICE_ATTR(ap_sleep_bp, JINGSHANG_DEV_MODE_MSK, gpio_jingshang_get_ap_sleep_bp, gpio_jingshang_set_ap_sleep_bp);


static ssize_t gpio_jingshang_get_rst(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_jingshang_get_general(device, JINGSHANG_GPIO_RST, buf);
}

static ssize_t gpio_jingshang_set_rst(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_jingshang_set_general(device, JINGSHANG_GPIO_RST, buf, count);
}

static DEVICE_ATTR(rst, JINGSHANG_DEV_MODE_MSK, gpio_jingshang_get_rst, gpio_jingshang_set_rst);


static ssize_t gpio_jingshang_get_sim_ctl0(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_jingshang_get_general(device, JINGSHANG_GPIO_SIM_CTL0, buf);
}

static ssize_t gpio_jingshang_set_sim_ctl0(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_jingshang_set_general(device, JINGSHANG_GPIO_SIM_CTL0, buf, count);
}

static DEVICE_ATTR(sim_ctl0, JINGSHANG_DEV_MODE_MSK, gpio_jingshang_get_sim_ctl0, gpio_jingshang_set_sim_ctl0);


static ssize_t gpio_jingshang_get_sim_ctl1(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_jingshang_get_general(device, JINGSHANG_GPIO_SIM_CTL1, buf);
}

static ssize_t gpio_jingshang_set_sim_ctl1(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_jingshang_set_general(device, JINGSHANG_GPIO_SIM_CTL1, buf, count);
}

static DEVICE_ATTR(sim_ctl1, JINGSHANG_DEV_MODE_MSK, gpio_jingshang_get_sim_ctl1, gpio_jingshang_set_sim_ctl1);


static ssize_t gpio_jingshang_get_sat_1p8_en(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_jingshang_get_general(device, JINGSHANG_GPIO_SAT_1P8_EN, buf);
}

static ssize_t gpio_jingshang_set_sat_1p8_en(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_jingshang_set_general(device, JINGSHANG_GPIO_SAT_1P8_EN, buf, count);
}

static DEVICE_ATTR(sat_1p8_en, JINGSHANG_DEV_MODE_MSK, gpio_jingshang_get_sat_1p8_en, gpio_jingshang_set_sat_1p8_en);


static ssize_t gpio_jingshang_get_tt_uart_en(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_jingshang_get_general(device, JINGSHANG_GPIO_TT_UART_EN, buf);
}

static ssize_t gpio_jingshang_set_tt_uart_en(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_jingshang_set_general(device, JINGSHANG_GPIO_TT_UART_EN, buf, count);
}

static DEVICE_ATTR(tt_uart_en, JINGSHANG_DEV_MODE_MSK, gpio_jingshang_get_tt_uart_en, gpio_jingshang_set_tt_uart_en);




static ssize_t reg_jingshang_set_general(struct device *device,
			int pwr_ctl_type,
			const char *buf, size_t count)
{
	bool enable = false;
	int ret_val = 0;
	struct jingshang_gpio_data *jingshang_data;
	jingshang_data = dev_get_drvdata(device);

	mutex_lock(&jingshang_data->lock);
	if (!strncmp(buf, "1", strlen("1"))) {
		enable = true;
	}
	else {
		enable = false;
	}

	if(pwr_ctl_type == JINGSHANG_REG_POW_CTL_1P1) {
		if (jingshang_data->vdd_1p1) {
			if(enable) {
				//regulator_set_voltage(jingshang_data->vdd_1p1, 1100000, 1100000);
				ret_val = regulator_enable(jingshang_data->vdd_1p1);
			}
			else {
				regulator_disable(jingshang_data->vdd_1p1);
			}
			if (ret_val < 0) {
				dev_err(device, "%s: Failed to enable power regulator %d\n", __func__, pwr_ctl_type);
			}
		}
		else {
			dev_err(device, "%s: no valid regulator %d\n", __func__, pwr_ctl_type);
		}
	}
	else if(pwr_ctl_type == JINGSHANG_REG_POW_CTL_3P3) {
		if (jingshang_data->vdd_3p3) {
			if(enable) {
				ret_val = regulator_enable(jingshang_data->vdd_3p3);
			}
			else {
				regulator_disable(jingshang_data->vdd_3p3);
			}
			if (ret_val < 0) {
				dev_err(device, "%s: Failed to enable power regulator %d\n", __func__, pwr_ctl_type);
			}
		}
		else {
			dev_err(device, "%s: no valid regulator %d\n", __func__, pwr_ctl_type);
		}
	}
	else {
		dev_err(device, "%s, unkown pwr_ctl_type %d\n", __func__, pwr_ctl_type);
	}

	mutex_unlock(&jingshang_data->lock);
	dev_info(device, "%s, type: %d, reg_ctl: %d, result: %d\n", __func__, pwr_ctl_type, enable, ret_val);
	return count;
}


static ssize_t reg_jingshang_read_general(struct device *device, int pwr_ctl_type, char *buf)
{
	int value = -1;
	struct regulator * reg = NULL;
	struct jingshang_gpio_data *jingshang_data;
	jingshang_data = dev_get_drvdata(device);

	mutex_lock(&jingshang_data->lock);
	jingshang_data = dev_get_drvdata(device);

	if(pwr_ctl_type == JINGSHANG_REG_POW_CTL_1P1){
		reg = jingshang_data->vdd_1p1;
	}
	else if(pwr_ctl_type == JINGSHANG_REG_POW_CTL_3P3) {
		reg = jingshang_data->vdd_3p3;
	}
	else {
		dev_err(device, "%s: no valid regulator %d\n", __func__, pwr_ctl_type);
	}

	if (reg) {
		value = regulator_get_voltage(reg);
		if (value < 0) {
			dev_err(device, "%s: Failed to get  valtage for regulator %d, vol = %d\n", __func__, pwr_ctl_type, value);
		}
	}

	mutex_unlock(&jingshang_data->lock);
	dev_info(device, "%s, reg_type [%d] get, value [%d]\n", __func__, pwr_ctl_type, value);

	return sysfs_emit(buf, "%d\n", value);

}



static ssize_t reg_jingshang_pwr_read_1p1(struct device *device,
                              struct device_attribute *attr, char *buf)
{
	return reg_jingshang_read_general(device, JINGSHANG_REG_POW_CTL_1P1, buf);
}

static ssize_t reg_jingshang_pwr_ctl_1p1(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return reg_jingshang_set_general(device, JINGSHANG_REG_POW_CTL_1P1, buf, count);
}

static DEVICE_ATTR(pwr_ctl_1p1, JINGSHANG_DEV_MODE_MSK, reg_jingshang_pwr_read_1p1, reg_jingshang_pwr_ctl_1p1);


static ssize_t reg_jingshang_pwr_read_3p3(struct device *device,
                              struct device_attribute *attr, char *buf)
{
	return reg_jingshang_read_general(device, JINGSHANG_REG_POW_CTL_3P3, buf);
}

static ssize_t reg_jingshang_pwr_ctl_3p3(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return reg_jingshang_set_general(device, JINGSHANG_REG_POW_CTL_3P3, buf, count);
}

static DEVICE_ATTR(pwr_ctl_3p3, JINGSHANG_DEV_MODE_MSK, reg_jingshang_pwr_read_3p3, reg_jingshang_pwr_ctl_3p3);


static struct attribute *attributes[] = {
	&dev_attr_bp_sleep_ap.attr,
	&dev_attr_ap_sleep_bp.attr,
	&dev_attr_rst.attr,
	&dev_attr_sim_ctl0.attr,
	&dev_attr_sim_ctl1.attr,
	&dev_attr_sat_1p8_en.attr,
	&dev_attr_tt_uart_en.attr,
	&dev_attr_pwr_ctl_1p1.attr,
	&dev_attr_pwr_ctl_3p3.attr,
	NULL
};

static const struct attribute_group attribute_group = {
	.attrs = attributes,
};


#define MAX_MSG_LENGTH 20
static void gpio_debounce_work(struct work_struct *work)
{
	struct jingshang_gpio_data *jingshang_data =
			container_of(work, struct jingshang_gpio_data, debounce_work.work);
	struct device *dev = jingshang_data->dev;
	char status_env[MAX_MSG_LENGTH];
	char *envp[] = { status_env, NULL };

	//only update changed gpio.
	if ( jingshang_data->data[JINGSHANG_GPIO_BP_SLEEP_AP].irq == jingshang_data->current_irq ) {
		int gpio_status = gpio_get_value(jingshang_data->data[JINGSHANG_GPIO_BP_SLEEP_AP].gpio_num);
		if (gpio_status == jingshang_data->data[JINGSHANG_GPIO_BP_SLEEP_AP].gpio_status) {
			snprintf(status_env, MAX_MSG_LENGTH, "STATUS=%d", gpio_status);
			dev_info(dev, "Update testing mode status: %d\n", gpio_status);
			kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
		}
	}
}

static irqreturn_t gpio_jingshang_threaded_irq_handler(int irq, void *irq_data)
{
	struct jingshang_gpio_data *jingshang_data = irq_data;
	struct device *dev = jingshang_data->dev;

	dev_info(dev, "irq [%d] triggered\n", irq);
	if ( irq == jingshang_data->data[JINGSHANG_GPIO_BP_SLEEP_AP].irq )
	{
		jingshang_data->data[JINGSHANG_GPIO_BP_SLEEP_AP].gpio_status = 
			gpio_get_value(jingshang_data->data[JINGSHANG_GPIO_BP_SLEEP_AP].gpio_num);
	}
	jingshang_data->current_irq = irq;

	mod_delayed_work(system_wq, &jingshang_data->debounce_work, msecs_to_jiffies(jingshang_data->debounce_time));

	return IRQ_HANDLED;
}


static int gpio_jingshang_reuqest_gpio(struct platform_device *pdev, 
	struct jingshang_gpio_data * jingshang_data, 
	const char* gpio_name)
{
	int i;
	int idx = -1;
	int ret = 0;
	int gpio_num = -1;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

        for(i = 0; i<JINGSHANG_GPIO_COUNT; i++) {
	        if(!strcmp(gpio_name, jingshang_gpio_names[i])) {
			idx = i;
			break;
	        	}
        	}

        if(idx == -1) {
		dev_err(dev, "Invalid gpio config name: %s",gpio_name);
		return -1;
	}

	gpio_num = of_get_named_gpio(np, gpio_name, 0);
	if (gpio_num < 0) {
		dev_err(dev, "Failed to get gpio %s, error: %d.\n", gpio_name, gpio_num);
	}
	ret = devm_gpio_request(dev, gpio_num, gpio_name);
	if (ret) {
		dev_err(dev, "Request gpio failed %s, error: %d.\n",  gpio_name, gpio_num);
		return ret;
	}
	dev_dbg(dev, "jingshang_gpio %s, #%u.\n", gpio_name, gpio_num);
	jingshang_data->data[idx].gpio_num = gpio_num;

	if(!strcmp(gpio_name, jingshang_gpio_names[0])) {
		gpio_direction_input(gpio_num);
		jingshang_data->data[idx].irq = gpio_to_irq(gpio_num);
		jingshang_data->data[idx].gpio_status = gpio_get_value(jingshang_data->data[idx].gpio_num);
		ret = devm_request_threaded_irq(dev, jingshang_data->data[idx].irq, NULL, gpio_jingshang_threaded_irq_handler, 
			IRQF_ONESHOT |IRQF_TRIGGER_FALLING, "gpio_jingshang", jingshang_data);
		if (ret < 0) {
			dev_err(dev, "Failed to request irq.\n");
			return -EINVAL;
		}
		dev_pm_set_wake_irq(dev, jingshang_data->data[idx].irq);
          }

	if((idx == JINGSHANG_GPIO_SIM_CTL0) ||(idx == JINGSHANG_GPIO_SIM_CTL1) ) {
		gpio_direction_output(gpio_num, 1);
          }

	return ret;
}


static int gpio_jingshang_reuqest_regulator(struct platform_device *pdev,
	struct jingshang_gpio_data * jingshang_data)
{
	int ret_val = 0;
	struct device *dev = &pdev->dev;

	 jingshang_data->vdd_1p1 = regulator_get(dev, "vdda1p1_vdd");
	if (IS_ERR(jingshang_data->vdd_1p1) ) {
		dev_dbg(dev, "%s: Failed to get power regulator 1p1\n", __func__);
		ret_val = PTR_ERR(jingshang_data->vdd_1p1);
		goto regulator_put2;
	}

	 jingshang_data->vdd_3p3 = regulator_get(dev, "vdda3p3_vdd");
	if (IS_ERR(jingshang_data->vdd_3p3)) {
		dev_dbg(dev, "%s: Failed to get power regulator 3p3\n", __func__);
		ret_val = PTR_ERR(jingshang_data->vdd_3p3);
		goto regulator_put1;
	}

	return ret_val;

regulator_put1:
	if (jingshang_data->vdd_3p3) {
		regulator_put(jingshang_data->vdd_3p3);
		jingshang_data->vdd_3p3 = NULL;
	}
regulator_put2:
	if (jingshang_data->vdd_1p1) {
		regulator_put(jingshang_data->vdd_1p1);
		jingshang_data->vdd_1p1 = NULL;
	}
	return ret_val;
}

static int gpio_jingshang_probe(struct platform_device *pdev)
{
	int i;
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct jingshang_gpio_data *jingshang_data;

	pr_info("%s.\n", __func__);

	jingshang_data = devm_kzalloc(dev, sizeof(struct jingshang_gpio_data), GFP_KERNEL);
	if (!jingshang_data) {
		dev_err(dev, "Memor allocation error!.\n");
		return -ENOMEM;
	}

	jingshang_data->data = devm_kcalloc(dev, JINGSHANG_GPIO_COUNT, sizeof(struct gpio_data), GFP_KERNEL);
	if (!jingshang_data->data) {
		dev_err(dev, "Memor allocation error!.\n");
		return -ENOMEM;
	}

	mutex_init(&jingshang_data->lock);
	jingshang_data->dev = dev;
	platform_set_drvdata(pdev, jingshang_data);

	for(i=0; i<JINGSHANG_GPIO_COUNT; i++) {
		gpio_jingshang_reuqest_gpio(pdev, jingshang_data, jingshang_gpio_names[i]);
	}

	gpio_jingshang_reuqest_regulator(pdev, jingshang_data);

	ret = sysfs_create_group(&dev->kobj, &attribute_group);
	if (ret < 0) {
		dev_err(dev, "Failed to create sysfs node.\n");
		return -EINVAL;
	}

	if ( of_property_read_u32(np, "debounce-time", &jingshang_data->debounce_time) ) {
		dev_info(dev, "Failed to get debounce-time, use default.\n");
		jingshang_data->debounce_time = 5;
	}

	device_init_wakeup(dev, true);
	INIT_DELAYED_WORK(&jingshang_data->debounce_work, gpio_debounce_work);

	return ret;
}

static int gpio_jingshang_remove(struct platform_device *pdev)
{
	struct jingshang_gpio_data *jingshang_data = platform_get_drvdata(pdev);

	//sysfs_remove_group(&pdev->dev.kobj, &attribute_group);
	//cancel_delayed_work_sync(&jingshang_data->debounce_work);
	dev_pm_clear_wake_irq(jingshang_data->dev);

	mutex_destroy(&jingshang_data->lock);

	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id gpio_jingshang_of_match[] = {
	{ .compatible = "mdm,gpio-jingshang", },
	{},
};
MODULE_DEVICE_TABLE(of, gpio_jingshang_of_match);
#endif


#ifdef CONFIG_PM
static int gpio_jingshang_suspend(struct device *dev)
{
	int gpio, rc;
	struct jingshang_gpio_data *jingshang_data;

	jingshang_data = dev_get_drvdata(dev);
	gpio = jingshang_data->data[JINGSHANG_GPIO_AP_SLEEP_BP].gpio_num;

	rc = gpio_direction_output(gpio, 1);
	dev_info(dev, "jingshang_suspend, %d.\n", rc);

	return 0;
}

static int gpio_jingshang_resume(struct device *dev)
{
	int gpio, rc;
	struct jingshang_gpio_data *jingshang_data;

	jingshang_data = dev_get_drvdata(dev);
	gpio = jingshang_data->data[JINGSHANG_GPIO_AP_SLEEP_BP].gpio_num;

	rc = gpio_direction_output(gpio, 0);
	dev_info(dev, "jingshang_resume, %d.\n", rc);

	return 0;
}

static const struct dev_pm_ops gpio_jingshang_pm_ops = {
	.suspend = gpio_jingshang_suspend,
	.resume = gpio_jingshang_resume,
	.freeze = gpio_jingshang_suspend,
	.restore = gpio_jingshang_resume,
};
#endif

static struct platform_driver gpio_jingshang_driver = {
	.driver = {
		.name = "gpio-jingshang",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(gpio_jingshang_of_match),
#ifdef CONFIG_PM
		.pm = &gpio_jingshang_pm_ops,
#endif
	},
	.probe = gpio_jingshang_probe,
	.remove = gpio_jingshang_remove,
};



static int __init gpio_jingshang_driver_init(void)
{
	int err;

	pr_info("%s.\n", __func__);
	err = platform_driver_register(&gpio_jingshang_driver);
	if (err) {
	    pr_err("%s error: %d\n", __func__, err);
	}

	return err;

}


static void __exit gpio_jingshang_driver_exit(void)
{
	platform_driver_unregister(&gpio_jingshang_driver);
}


module_init(gpio_jingshang_driver_init);
module_exit(gpio_jingshang_driver_exit);


//module_platform_driver(gpio_jingshang_driver);
MODULE_AUTHOR("huangqianhong@xiaomi.com");
MODULE_DESCRIPTION("Driver for jingshang GPIO control");
MODULE_LICENSE("GPL v2");


