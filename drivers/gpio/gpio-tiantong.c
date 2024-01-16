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

#define TIANTONG_DUAL_SIM_SUPPORT

#ifndef TIANTONG_DUAL_SIM_SUPPORT
#define TIANTONG_GPIO_COUNT 9
#else
#define TIANTONG_GPIO_COUNT 14
#endif

#define TIANTONG_DEV_MODE_MSK (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH)	/*664*/

enum {
	TIANTON_GPIO_BP_SLEEP_AP = 0,
	TIANTON_GPIO_AP_SLEEP_BP,
	TIANTON_GPIO_RST,
	TIANTON_GPIO_BOOT_MODE0,
	TIANTON_GPIO_BOOT_MODE3,
	TIANTON_GPIO_SAT_1P1_EN,
	TIANTON_GPIO_SAT_1P8_EN,
	TIANTON_GPIO_TT_UART_EN,
#ifndef TIANTONG_DUAL_SIM_SUPPORT
	TIANTON_GPIO_SIM_SWITCH,
#else
	TIANTON_GPIO_SIM_SIGNAL_1,
	TIANTON_GPIO_SIM_SIGNAL_2,
	TIANTON_GPIO_SIM_POWER_1_QC,
	TIANTON_GPIO_SIM_POWER_1_NTN,
	TIANTON_GPIO_SIM_POWER_2_QC,
	TIANTON_GPIO_SIM_POWER_2_NTN,
#endif
};

enum {
	TIANTON_REG_POW_CTL_1P2 = 0,
	TIANTON_REG_POW_CTL_3P3
};


static const char * const tiantong_gpio_names[] = {
	"gpio-bq-sleep-ap",
	"gpio-aq-sleep-bp",
	"gpio-rst",
	"gpio-boot-mode0",
	"gpio-boot-mode3",
	"gpio-sat-1p1-en",
	"gpio-sat-1p8-en",
	"gpio-tt-uart-en",
#ifndef TIANTONG_DUAL_SIM_SUPPORT
	"gpio-sim-switch",
#else
	"gpio-sim-signal-1",
	"gpio-sim-signal-2",
	"gpio-sim-power-1-qc",
	"gpio-sim-power-1-ntn",
	"gpio-sim-power-2-qc",
	"gpio-sim-power-2-ntn",
#endif
};

struct gpio_data {
	int irq;
	int gpio_num;
	int gpio_status;
};

struct tiantong_gpio_data {
	struct device *dev;
	int debounce_time;
	int current_irq;

	struct mutex lock;	/* To set/get exported values in sysfs */

	struct delayed_work debounce_work;
	struct gpio_data *data;
	struct regulator *vdd_1p2;
	struct regulator *vdd_3p3;
};


static ssize_t gpio_tiantong_set_general(struct device *device, int gpio_type, const char *buf, size_t count)
{
	int rc = -1;
	int gpio = 0;
	struct tiantong_gpio_data *tiantong_data;

	tiantong_data = dev_get_drvdata(device);
	gpio = tiantong_data->data[gpio_type].gpio_num;

	mutex_lock(&tiantong_data->lock);
	if (!strncmp(buf, "1", strlen("1"))) {
		if (gpio_is_valid(gpio)) {
			rc = gpio_direction_output(gpio, 1);
			if (rc) {
				dev_err(device, "tiantong %s: fail to set gpio %d !\n", __func__, gpio);
				goto unlock_ret;
			}
		} else {
			dev_err(device, "tiantong %s: unable to get gpio %d!\n", __func__, gpio);
		}
	} 
	else if (!strncmp(buf, "0", strlen("0"))) {
		if (gpio_is_valid(gpio)) {
			rc = gpio_direction_output(gpio, 0);
			if (rc) {
				dev_err(device, "tiantong %s: fail to set gpio %d !\n", __func__, gpio);
				goto unlock_ret;
			}
		} else {
			dev_err(device, "tiantong %s: unable to get gpio %d!\n", __func__, gpio);
		}
	} 
	else 
	{
		rc = -EINVAL;
		dev_err(device, "tiantong %s: invalid input %s!only 0 or 1 is valid.\n", __func__, buf);
	}

unlock_ret:
	mutex_unlock(&tiantong_data->lock);
	dev_info(device, "%s, gpio_type [%d] set, rc [%d], buf = %s\n", __func__, gpio_type, rc, buf);
	return count;
}



static ssize_t gpio_tiantong_get_general(struct device *device, int gpio_type, char *buf)
{
	int value = -1;
	int gpio = 0;
	struct tiantong_gpio_data *tiantong_data;
	tiantong_data = dev_get_drvdata(device);

	mutex_lock(&tiantong_data->lock);
	tiantong_data = dev_get_drvdata(device);
	gpio = tiantong_data->data[gpio_type].gpio_num;

	if (gpio_is_valid(gpio)) {
		value = gpio_get_value(gpio);
	} else {
		dev_err(device, "tiantong %s: unable to get gpio %d!\n", __func__, gpio);
	}
	mutex_unlock(&tiantong_data->lock);

	dev_info(device, "%s, gpio_type [%d] get, value [%d]\n", __func__, gpio_type, value);
	return sysfs_emit(buf, "%d\n", value);
}



static ssize_t gpio_tiantong_get_bp_sleep_ap(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_tiantong_get_general(device, TIANTON_GPIO_BP_SLEEP_AP, buf);
}

static ssize_t gpio_tiantong_set_bp_sleep_ap(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_tiantong_set_general(device, TIANTON_GPIO_BP_SLEEP_AP, buf, count);
}

static DEVICE_ATTR(bp_sleep_ap, TIANTONG_DEV_MODE_MSK, gpio_tiantong_get_bp_sleep_ap, gpio_tiantong_set_bp_sleep_ap);

static ssize_t gpio_tiantong_get_ap_sleep_bp(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_tiantong_get_general(device, TIANTON_GPIO_AP_SLEEP_BP, buf);
}

static ssize_t gpio_tiantong_set_ap_sleep_bp(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_tiantong_set_general(device, TIANTON_GPIO_AP_SLEEP_BP, buf, count);
}

static DEVICE_ATTR(ap_sleep_bp, TIANTONG_DEV_MODE_MSK, gpio_tiantong_get_ap_sleep_bp, gpio_tiantong_set_ap_sleep_bp);


static ssize_t gpio_tiantong_get_rst(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_tiantong_get_general(device, TIANTON_GPIO_RST, buf);
}

static ssize_t gpio_tiantong_set_rst(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_tiantong_set_general(device, TIANTON_GPIO_RST, buf, count);
}

static DEVICE_ATTR(rst, TIANTONG_DEV_MODE_MSK, gpio_tiantong_get_rst, gpio_tiantong_set_rst);

static ssize_t gpio_tiantong_get_boot_mode0(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_tiantong_get_general(device, TIANTON_GPIO_BOOT_MODE0, buf);
}

static ssize_t gpio_tiantong_set_boot_mode0(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_tiantong_set_general(device, TIANTON_GPIO_BOOT_MODE0, buf, count);
}

static DEVICE_ATTR(boot_mode0, TIANTONG_DEV_MODE_MSK, gpio_tiantong_get_boot_mode0, gpio_tiantong_set_boot_mode0);


//static DEVICE_ATTR(boot_mode1, TIANTONG_DEV_MODE_MSK);


//static DEVICE_ATTR(boot_mode2, TIANTONG_DEV_MODE_MSK);

static ssize_t gpio_tiantong_get_boot_mode3(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_tiantong_get_general(device, TIANTON_GPIO_BOOT_MODE3, buf);
}

static ssize_t gpio_tiantong_set_boot_mode3(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_tiantong_set_general(device, TIANTON_GPIO_BOOT_MODE3, buf, count);
}

static DEVICE_ATTR(boot_mode3, TIANTONG_DEV_MODE_MSK, gpio_tiantong_get_boot_mode3, gpio_tiantong_set_boot_mode3);

static ssize_t gpio_tiantong_get_sat_1p1_en(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_tiantong_get_general(device, TIANTON_GPIO_SAT_1P1_EN, buf);
}

static ssize_t gpio_tiantong_set_sat_1p1_en(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_tiantong_set_general(device, TIANTON_GPIO_SAT_1P1_EN, buf, count);
}

static DEVICE_ATTR(sat_1p1_en, TIANTONG_DEV_MODE_MSK, gpio_tiantong_get_sat_1p1_en, gpio_tiantong_set_sat_1p1_en);


static ssize_t gpio_tiantong_get_sat_1p8_en(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_tiantong_get_general(device, TIANTON_GPIO_SAT_1P8_EN, buf);
}

static ssize_t gpio_tiantong_set_sat_1p8_en(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_tiantong_set_general(device, TIANTON_GPIO_SAT_1P8_EN, buf, count);
}

static DEVICE_ATTR(sat_1p8_en, TIANTONG_DEV_MODE_MSK, gpio_tiantong_get_sat_1p8_en, gpio_tiantong_set_sat_1p8_en);


static ssize_t gpio_tiantong_get_tt_uart_en(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_tiantong_get_general(device, TIANTON_GPIO_TT_UART_EN, buf);
}

static ssize_t gpio_tiantong_set_tt_uart_en(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_tiantong_set_general(device, TIANTON_GPIO_TT_UART_EN, buf, count);
}

static DEVICE_ATTR(tt_uart_en, TIANTONG_DEV_MODE_MSK, gpio_tiantong_get_tt_uart_en, gpio_tiantong_set_tt_uart_en);


#ifndef TIANTONG_DUAL_SIM_SUPPORT
static ssize_t gpio_tiantong_get_sim_switch(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_tiantong_get_general(device, TIANTON_GPIO_SIM_SWITCH, buf);
}

static ssize_t gpio_tiantong_set_sim_switch(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_tiantong_set_general(device, TIANTON_GPIO_SIM_SWITCH, buf, count);
}

static DEVICE_ATTR(sim_switch, TIANTONG_DEV_MODE_MSK, gpio_tiantong_get_sim_switch, gpio_tiantong_set_sim_switch);

#else

static ssize_t gpio_tiantong_get_sim_signal1(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_tiantong_get_general(device, TIANTON_GPIO_SIM_SIGNAL_1, buf);
}

static ssize_t gpio_tiantong_set_sim_signal1(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_tiantong_set_general(device, TIANTON_GPIO_SIM_SIGNAL_1, buf, count);
}

static DEVICE_ATTR(sim_signal1, TIANTONG_DEV_MODE_MSK, gpio_tiantong_get_sim_signal1, gpio_tiantong_set_sim_signal1);


static ssize_t gpio_tiantong_get_sim_signal2(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_tiantong_get_general(device, TIANTON_GPIO_SIM_SIGNAL_2, buf);
}

static ssize_t gpio_tiantong_set_sim_signal2(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_tiantong_set_general(device, TIANTON_GPIO_SIM_SIGNAL_2, buf, count);
}

static DEVICE_ATTR(sim_signal2, TIANTONG_DEV_MODE_MSK, gpio_tiantong_get_sim_signal2, gpio_tiantong_set_sim_signal2);


static ssize_t gpio_tiantong_get_sim_power1_qc(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_tiantong_get_general(device, TIANTON_GPIO_SIM_POWER_1_QC, buf);
}

static ssize_t gpio_tiantong_set_sim_power1_qc(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_tiantong_set_general(device, TIANTON_GPIO_SIM_POWER_1_QC, buf, count);
}

static DEVICE_ATTR(sim_power1_qc, TIANTONG_DEV_MODE_MSK, gpio_tiantong_get_sim_power1_qc, gpio_tiantong_set_sim_power1_qc);

static ssize_t gpio_tiantong_get_sim_power1_ntn(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_tiantong_get_general(device, TIANTON_GPIO_SIM_POWER_1_NTN, buf);
}

static ssize_t gpio_tiantong_set_sim_power1_ntn(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_tiantong_set_general(device, TIANTON_GPIO_SIM_POWER_1_NTN, buf, count);
}

static DEVICE_ATTR(sim_power1_ntn, TIANTONG_DEV_MODE_MSK, gpio_tiantong_get_sim_power1_ntn, gpio_tiantong_set_sim_power1_ntn);


static ssize_t gpio_tiantong_get_sim_power2_qc(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_tiantong_get_general(device, TIANTON_GPIO_SIM_POWER_2_QC, buf);
}

static ssize_t gpio_tiantong_set_sim_power2_qc(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_tiantong_set_general(device, TIANTON_GPIO_SIM_POWER_2_QC, buf, count);
}

static DEVICE_ATTR(sim_power2_qc, TIANTONG_DEV_MODE_MSK, gpio_tiantong_get_sim_power2_qc, gpio_tiantong_set_sim_power2_qc);

static ssize_t gpio_tiantong_get_sim_power2_ntn(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return gpio_tiantong_get_general(device, TIANTON_GPIO_SIM_POWER_2_NTN, buf);
}

static ssize_t gpio_tiantong_set_sim_power2_ntn(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return gpio_tiantong_set_general(device, TIANTON_GPIO_SIM_POWER_2_NTN, buf, count);
}

static DEVICE_ATTR(sim_power2_ntn, TIANTONG_DEV_MODE_MSK, gpio_tiantong_get_sim_power2_ntn, gpio_tiantong_set_sim_power2_ntn);


#endif


static ssize_t reg_tiantong_set_general(struct device *device,
			int pwr_ctl_type,
			const char *buf, size_t count)
{
	bool enable = false;
	int ret_val = 0;
	struct tiantong_gpio_data *tiantong_data;
	tiantong_data = dev_get_drvdata(device);

	mutex_lock(&tiantong_data->lock);
	if (!strncmp(buf, "1", strlen("1"))) {
		enable = true;
	}
	else {
		enable = false;
	}

	if(pwr_ctl_type == TIANTON_REG_POW_CTL_1P2) {
		if (tiantong_data->vdd_1p2) {
			if(enable) {
				ret_val = regulator_enable(tiantong_data->vdd_1p2);
			}
			else {
				regulator_disable(tiantong_data->vdd_1p2);
			}
			if (ret_val < 0) {
				dev_info(device, "%s: Failed to enable power regulator %d\n", __func__, pwr_ctl_type);
			}
		}
		else {
			dev_info(device, "%s: no valid regulator %d\n", __func__, pwr_ctl_type);
		}
	}
	else if(pwr_ctl_type == TIANTON_REG_POW_CTL_3P3) {
		if (tiantong_data->vdd_3p3) {
			if(enable) {
				ret_val = regulator_enable(tiantong_data->vdd_3p3);
			}
			else {
				regulator_disable(tiantong_data->vdd_3p3);
			}
			if (ret_val < 0) {
				dev_info(device, "%s: Failed to enable power regulator %d\n", __func__, pwr_ctl_type);
			}
		}
		else {
			dev_info(device, "%s: no valid regulator %d\n", __func__, pwr_ctl_type);
		}
	}
	else {
		dev_info(device, "%s, unkown pwr_ctl_type %d\n", __func__, pwr_ctl_type);
	}

	mutex_unlock(&tiantong_data->lock);
	dev_info(device, "%s, type: %d, reg_ctl: %d, result: %d\n", __func__, pwr_ctl_type, enable, ret_val);
	return ret_val = 0;
}


static ssize_t reg_tiantong_read_general(struct device *device, int pwr_ctl_type, char *buf)
{
	int value = -1;
	struct regulator * reg = NULL;
	struct tiantong_gpio_data *tiantong_data;
	tiantong_data = dev_get_drvdata(device);

	mutex_lock(&tiantong_data->lock);
	tiantong_data = dev_get_drvdata(device);

	if(pwr_ctl_type == TIANTON_REG_POW_CTL_1P2){
		reg = tiantong_data->vdd_1p2;
	}
	else if(pwr_ctl_type == TIANTON_REG_POW_CTL_3P3) {
		reg = tiantong_data->vdd_3p3;
	}
	else {
		dev_info(device, "%s: no valid regulator %d\n", __func__, pwr_ctl_type);
	}

	if (reg) {
		value = regulator_get_voltage(reg);
		if (value < 0) {
			dev_info(device, "%s: Failed to get  valtage for regulator %d, vol = %d\n", __func__, pwr_ctl_type, value);
		}
	}
	scnprintf(buf, PAGE_SIZE, "%d\n", value);

	mutex_unlock(&tiantong_data->lock);
	dev_info(device, "%s, reg_type [%d] get, value [%d], buf = %s\n", __func__, pwr_ctl_type, value, buf);
	return value = 0;
}



static ssize_t reg_tiantong_pwr_read_1p2(struct device *device,
                              struct device_attribute *attr, char *buf)
{
	return reg_tiantong_read_general(device, TIANTON_REG_POW_CTL_1P2, buf);
}

static ssize_t reg_tiantong_pwr_ctl_1p2(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return reg_tiantong_set_general(device, TIANTON_REG_POW_CTL_1P2, buf, count);
}

static DEVICE_ATTR(pwr_ctl_1p2, TIANTONG_DEV_MODE_MSK, reg_tiantong_pwr_read_1p2, reg_tiantong_pwr_ctl_1p2);


static ssize_t reg_tiantong_pwr_read_3p3(struct device *device,
                              struct device_attribute *attr, char *buf)
{
	return reg_tiantong_read_general(device, TIANTON_REG_POW_CTL_3P3, buf);
}

static ssize_t reg_tiantong_pwr_ctl_3p3(struct device *device,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return reg_tiantong_set_general(device, TIANTON_REG_POW_CTL_3P3, buf, count);
}

static DEVICE_ATTR(pwr_ctl_3p3, TIANTONG_DEV_MODE_MSK, reg_tiantong_pwr_read_3p3, reg_tiantong_pwr_ctl_3p3);


static struct attribute *attributes[] = {
	&dev_attr_bp_sleep_ap.attr,
	&dev_attr_ap_sleep_bp.attr,
	&dev_attr_rst.attr,
	&dev_attr_boot_mode0.attr,
	&dev_attr_boot_mode3.attr,
	&dev_attr_sat_1p1_en.attr,
	&dev_attr_sat_1p8_en.attr,
	&dev_attr_tt_uart_en.attr,
#ifndef TIANTONG_DUAL_SIM_SUPPORT
	&dev_attr_sim_switch.attr,
#else
	&dev_attr_sim_signal1.attr,
	&dev_attr_sim_signal2.attr,
	&dev_attr_sim_power1_qc.attr,
	&dev_attr_sim_power1_ntn.attr,
	&dev_attr_sim_power2_qc.attr,
	&dev_attr_sim_power2_ntn.attr,
#endif
	&dev_attr_pwr_ctl_1p2.attr,
	&dev_attr_pwr_ctl_3p3.attr,
	NULL
};

static const struct attribute_group attribute_group = {
	.attrs = attributes,
};


#define MAX_MSG_LENGTH 20
static void gpio_debounce_work(struct work_struct *work)
{
	struct tiantong_gpio_data *tiantong_data =
			container_of(work, struct tiantong_gpio_data, debounce_work.work);
	struct device *dev = tiantong_data->dev;
	char status_env[MAX_MSG_LENGTH];
	char *envp[] = { status_env, NULL };

	//only update changed gpio.
	if ( tiantong_data->data[TIANTON_GPIO_BP_SLEEP_AP].irq == tiantong_data->current_irq ) {
		int gpio_status = gpio_get_value(tiantong_data->data[TIANTON_GPIO_BP_SLEEP_AP].gpio_num);
		if (gpio_status == tiantong_data->data[TIANTON_GPIO_BP_SLEEP_AP].gpio_status) {
			snprintf(status_env, MAX_MSG_LENGTH, "STATUS=%d", gpio_status);
			dev_info(dev, "Update testing mode status: %d\n", gpio_status);
			kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
		}
	}
}

static irqreturn_t gpio_tiantong_threaded_irq_handler(int irq, void *irq_data)
{
	struct tiantong_gpio_data *tiantong_data = irq_data;
	struct device *dev = tiantong_data->dev;

	dev_info(dev, "irq [%d] triggered\n", irq);
	if ( irq == tiantong_data->data[TIANTON_GPIO_BP_SLEEP_AP].irq )
	{
		tiantong_data->data[TIANTON_GPIO_BP_SLEEP_AP].gpio_status = 
			gpio_get_value(tiantong_data->data[TIANTON_GPIO_BP_SLEEP_AP].gpio_num);
	}
	tiantong_data->current_irq = irq;

	mod_delayed_work(system_wq, &tiantong_data->debounce_work, msecs_to_jiffies(tiantong_data->debounce_time));

	return IRQ_HANDLED;
}


static int gpio_tiantong_reuqest_gpio(struct platform_device *pdev, 
	struct tiantong_gpio_data * tiantong_data, 
	const char* gpio_name)
{
	int i;
	int idx = -1;
	int ret = 0;
	int gpio_num = -1;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

        for(i = 0; i<TIANTONG_GPIO_COUNT; i++) {
	        if(!strcmp(gpio_name, tiantong_gpio_names[i])) {
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
	dev_dbg(dev, "tiantong_gpio %s, #%u.\n", gpio_name, gpio_num);
	tiantong_data->data[idx].gpio_num = gpio_num;

	if(!strcmp(gpio_name, tiantong_gpio_names[0])) {
		gpio_direction_input(gpio_num);
		tiantong_data->data[idx].irq = gpio_to_irq(gpio_num);
		tiantong_data->data[idx].gpio_status = gpio_get_value(tiantong_data->data[idx].gpio_num);
		ret = devm_request_threaded_irq(dev, tiantong_data->data[idx].irq, NULL, gpio_tiantong_threaded_irq_handler, 
			IRQF_ONESHOT |IRQF_TRIGGER_FALLING, "gpio_tiantong", tiantong_data);
		if (ret < 0) {
			dev_err(dev, "Failed to request irq.\n");
			return -EINVAL;
		}
		dev_pm_set_wake_irq(dev, tiantong_data->data[idx].irq);
          }

#ifdef TIANTONG_DUAL_SIM_SUPPORT
	if((idx == TIANTON_GPIO_SIM_POWER_1_QC) ||(idx == TIANTON_GPIO_SIM_POWER_2_QC) ) {
		gpio_direction_output(gpio_num, 1);
          }
#endif

	return ret;
}


static int gpio_tiantong_reuqest_regulator(struct platform_device *pdev,
	struct tiantong_gpio_data * tiantong_data)
{
	int ret_val = 0;
	struct device *dev = &pdev->dev;

	 tiantong_data->vdd_1p2 = regulator_get(dev, "vdda1p2_vdd");
	if (IS_ERR(tiantong_data->vdd_1p2) ) {
		dev_dbg(dev, "%s: Failed to get power regulator 1p2\n", __func__);
		ret_val = PTR_ERR(tiantong_data->vdd_1p2);
		goto regulator_put2;
	}

	 tiantong_data->vdd_3p3 = regulator_get(dev, "vdda3p3_vdd");
	if (IS_ERR(tiantong_data->vdd_3p3)) {
		dev_dbg(dev, "%s: Failed to get power regulator 3p3\n", __func__);
		ret_val = PTR_ERR(tiantong_data->vdd_3p3);
		goto regulator_put1;
	}

	return ret_val;

regulator_put1:
	if (tiantong_data->vdd_3p3) {
		regulator_put(tiantong_data->vdd_3p3);
		tiantong_data->vdd_3p3 = NULL;
	}
regulator_put2:
	if (tiantong_data->vdd_1p2) {
		regulator_put(tiantong_data->vdd_1p2);
		tiantong_data->vdd_1p2 = NULL;
	}
	return ret_val;
}

static int gpio_tiantong_probe(struct platform_device *pdev)
{
	int i;
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct tiantong_gpio_data *tiantong_data;

	pr_info("%s enter\n", __func__);

	tiantong_data = devm_kzalloc(dev, sizeof(struct tiantong_gpio_data), GFP_KERNEL);
	if (!tiantong_data)
		return -ENOMEM;

	tiantong_data->data = devm_kcalloc(dev, TIANTONG_GPIO_COUNT, sizeof(struct gpio_data), GFP_KERNEL);
	if (!tiantong_data->data)
		return -ENOMEM;

	mutex_init(&tiantong_data->lock);
	tiantong_data->dev = dev;
	platform_set_drvdata(pdev, tiantong_data);

	for(i=0; i<TIANTONG_GPIO_COUNT; i++) {
		gpio_tiantong_reuqest_gpio(pdev, tiantong_data, tiantong_gpio_names[i]);
	}

	gpio_tiantong_reuqest_regulator(pdev, tiantong_data);

	ret = sysfs_create_group(&dev->kobj, &attribute_group);
	if (ret < 0) {
		dev_err(dev, "Failed to create sysfs node.\n");
		return -EINVAL;
	}

	if ( of_property_read_u32(np, "debounce-time", &tiantong_data->debounce_time) ) {
		dev_info(dev, "Failed to get debounce-time, use default.\n");
		tiantong_data->debounce_time = 5;
	}

	device_init_wakeup(dev, true);
	INIT_DELAYED_WORK(&tiantong_data->debounce_work, gpio_debounce_work);

	return ret;
}

static int gpio_tiantong_remove(struct platform_device *pdev)
{
	struct tiantong_gpio_data *tiantong_data = platform_get_drvdata(pdev);

	//sysfs_remove_group(&pdev->dev.kobj, &attribute_group);
	//cancel_delayed_work_sync(&tiantong_data->debounce_work);
	dev_pm_clear_wake_irq(tiantong_data->dev);

	mutex_destroy(&tiantong_data->lock);

	return 0;
}

static const struct of_device_id gpio_tiantong_of_match[] = {
	{ .compatible = "modem,gpio-tiantong", },
	{},
};

#ifdef CONFIG_PM
static int gpio_tiantong_suspend(struct device *dev)
{
	int gpio, rc;
	struct tiantong_gpio_data *tiantong_data;

	tiantong_data = dev_get_drvdata(dev);
	gpio = tiantong_data->data[TIANTON_GPIO_AP_SLEEP_BP].gpio_num;

	rc = gpio_direction_output(gpio, 1);
	dev_info(dev, "tiantong_suspend, %d.\n", rc);

	return 0;
}

static int gpio_tiantong_resume(struct device *dev)
{
	int gpio, rc;
	struct tiantong_gpio_data *tiantong_data;

	tiantong_data = dev_get_drvdata(dev);
	gpio = tiantong_data->data[TIANTON_GPIO_AP_SLEEP_BP].gpio_num;

	rc = gpio_direction_output(gpio, 0);
	dev_info(dev, "tiantong_resume, %d.\n", rc);

	return 0;
}

static const struct dev_pm_ops gpio_tiantong_pm_ops = {
	.suspend = gpio_tiantong_suspend,
	.resume = gpio_tiantong_resume,
	.freeze = gpio_tiantong_suspend,
	.restore = gpio_tiantong_resume,
};
#endif

static struct platform_driver gpio_tiantong_driver = {
	.driver = {
		.name = "gpio-tiantong",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(gpio_tiantong_of_match),
#ifdef CONFIG_PM
		.pm = &gpio_tiantong_pm_ops,
#endif
	},
	.probe = gpio_tiantong_probe,
	.remove = gpio_tiantong_remove,
};

module_platform_driver(gpio_tiantong_driver);
MODULE_DESCRIPTION("Driver for tiantong GPIO control");
MODULE_LICENSE("GPL");


