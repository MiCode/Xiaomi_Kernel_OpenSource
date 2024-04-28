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
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>

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

	/*char dev related data */
	char *driver_name;
	dev_t jingshang_major;
	struct class *jingshang_class;
	struct device *chardev;
	struct cdev cdev;
};


/*
TT_SOC_VDD =    0, L1M
TT_SOC_IOVDD = 1, L3M
TT_RFIC_ADC 	= 2, L4M
TT_RF_AVCC 	= 3, L7M
*/
typedef enum {
	TT_SOC_VDD = 0, // 1p1
	TT_SOC_IOVDD = 1, // 1p8
	TT_RFIC_VCC 	= 2,
	TT_RFIC_AVCC = 3, // 3p3
	TT_MAX,
 }tt_chip_ldo_type;

struct tt_chip_ldo_voltage_set_value {
	tt_chip_ldo_type 	ldo_type;
	int 				min_vol_val;
	int 				max_vol_val;
};

struct tt_chip_ldo_voltage_get_value {
	tt_chip_ldo_type 	ldo_type;
	int 				vol_val;
};

struct tt_chip_ldo_state {
	tt_chip_ldo_type	ldo_type;
	unsigned char		state;
};


#define MINOR_NUMBER_COUNT 1

#define TT_IOC_MAGIC 'y'
#define TT_VOLTAGE_ENABLE		_IOWR(TT_IOC_MAGIC, 0, int)
#define TT_VOLTAGE_DISABLE		_IOWR(TT_IOC_MAGIC, 1, int)
#define TT_VOLTAGE_SET			_IOWR(TT_IOC_MAGIC, 2, struct tt_chip_ldo_voltage_set_value)
#define TT_VOLTAGE_GET			_IOWR(TT_IOC_MAGIC, 3, struct tt_chip_ldo_voltage_get_value)
#define TT_CLK_ENABLE			_IOWR(TT_IOC_MAGIC, 4, int)
#define TT_CLK_DISABLE			_IOWR(TT_IOC_MAGIC, 5, int)
#define TT_RST_GPIO_SET_VALUE	_IOWR(TT_IOC_MAGIC, 6, int)
#define TT_AP2CP_GPIO_SET_VALUE	_IOWR(TT_IOC_MAGIC, 7, int)
#define TT_STATE_GET				_IOWR(TT_IOC_MAGIC, 8, struct tt_chip_ldo_state)
#define TT_CP2AP_GPIO_GET_VALUE	_IOWR(TT_IOC_MAGIC, 9, int)
#define TT_TT_UART_EN_SET_VALUE	_IOWR(TT_IOC_MAGIC, 10, int)


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
				pr_info("jingshang JINGSHANG_REG_POW_CTL_1P1 1111 for %d.\n", pwr_ctl_type);
			}
			else {
				regulator_disable(jingshang_data->vdd_1p1);
                                pr_info("jingshang JINGSHANG_REG_POW_CTL_1P1 2222 for %d.\n", pwr_ctl_type);
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
				pr_info("jingshang JINGSHANG_REG_POW_CTL_3P3 1111 for %d.\n", pwr_ctl_type);
			}
			else {
				regulator_disable(jingshang_data->vdd_3p3);
				pr_info("jingshang JINGSHANG_REG_POW_CTL_3P3 2222 for %d.\n", pwr_ctl_type);
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


static int jingshang_gpio_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct jingshang_gpio_data *jingshang_data = container_of(inode->i_cdev,
						struct jingshang_gpio_data,
						cdev);
	struct device *dev = jingshang_data->chardev;

	pr_info("Inside %s\n", __func__);
	get_device(dev);
	return ret;
}

static int jingshang_gpio_release(struct inode *inode, struct file *file)
{
	struct jingshang_gpio_data *jingshang_data = container_of(inode->i_cdev,
						struct jingshang_gpio_data,
						cdev);
	struct device *dev = jingshang_data->chardev;

	pr_info("Inside %s\n", __func__);
	put_device(dev);
	return 0;
}

static long jingshang_gpio_ioctl(struct file *file, unsigned int ioctl_num,
				unsigned long __user ioctl_param)
{
	int ret = 0;

	void __user *argp = (void __user *)ioctl_param;
	int __user *p = argp;
	//struct tt_chip_ldo_voltage_get_value __user *p_get_voltage = argp;
	struct tt_chip_ldo_voltage_set_value  __user *p_set_voltage = argp;
	int gpio_num;
	int gpio_value = 0;
	int tt_voltage_type = 0;
	//struct tt_chip_ldo_voltage_get_value voltage_get_buf;
	struct tt_chip_ldo_voltage_set_value voltage_set_buf;

	struct jingshang_gpio_data *jingshang_data =
			container_of(file->f_inode->i_cdev, struct jingshang_gpio_data, cdev);
	struct tt_chip_ldo_state ldo_data;
	pr_info("%s ioctl num %u\n", __func__, ioctl_num);
	switch (ioctl_num) {
	case TT_VOLTAGE_ENABLE:
		get_user(tt_voltage_type, p);
		pr_info("jingshang TT_VOLTAGE_ENABLE  for %d.\n", tt_voltage_type);
		if (tt_voltage_type == TT_SOC_VDD) {
			if(jingshang_data->vdd_1p1) {
				ret = regulator_enable(jingshang_data->vdd_1p1);
			}
		}
		else if (tt_voltage_type == TT_RFIC_AVCC)  {
			if (jingshang_data->vdd_3p3) {
				ret = regulator_enable(jingshang_data->vdd_3p3);
			}
		}
		else if (tt_voltage_type == TT_SOC_IOVDD) {
			if(jingshang_data->data) {
				gpio_num = jingshang_data->data[JINGSHANG_GPIO_SAT_1P8_EN].gpio_num;
				gpio_direction_output(gpio_num, 1);
			}
		}
		break;

	case TT_VOLTAGE_DISABLE:
		get_user(tt_voltage_type, p);
		pr_info("jingshang TT_VOLTAGE_DISABLE  for %d.\n", tt_voltage_type);
		if (tt_voltage_type == TT_SOC_VDD) {
			if((jingshang_data->vdd_1p1) && regulator_is_enabled(jingshang_data->vdd_1p1)) {
				pr_info("jingshang TT_VOLTAGE_DISABLE 1 for %d.\n", tt_voltage_type);
				ret = regulator_disable(jingshang_data->vdd_1p1);
				pr_info("jingshang TT_VOLTAGE_DISABLE ret %d.\n", ret);
			}
		}
		else if (tt_voltage_type == TT_RFIC_AVCC)  {
			if ((jingshang_data->vdd_3p3) && regulator_is_enabled(jingshang_data->vdd_3p3)) {
				pr_info("jingshang TT_VOLTAGE_DISABLE 2 for %d.\n", tt_voltage_type);
				ret = regulator_disable(jingshang_data->vdd_3p3);
				pr_info("jingshang TT_VOLTAGE_DISABLE ret %d.\n", ret);
			}
		}
		else if (tt_voltage_type == TT_SOC_IOVDD) {
			if(jingshang_data->data) {
				gpio_num = jingshang_data->data[JINGSHANG_GPIO_SAT_1P8_EN].gpio_num;
				gpio_direction_output(gpio_num, 0);
			}
		}
		break;

	case TT_VOLTAGE_SET:
		get_user(voltage_set_buf.ldo_type, &p_set_voltage->ldo_type);
		pr_info("jingshang VOLTAGE_SET %d, %d !\n", voltage_set_buf.ldo_type, voltage_set_buf.max_vol_val);
		if((voltage_set_buf.ldo_type == TT_SOC_VDD) && (jingshang_data->vdd_3p3)) {
			regulator_set_voltage(jingshang_data->vdd_1p1, 1100000, 1100000);
		}
		else if((voltage_set_buf.ldo_type == TT_RFIC_AVCC) && (jingshang_data->vdd_3p3)){
			regulator_set_voltage(jingshang_data->vdd_3p3, 3300000, 3300000);
			pr_info("jingshang %d: VOLTAGE_SET %d !\n", voltage_set_buf.ldo_type, voltage_set_buf.max_vol_val);
		}
		break;

	case TT_VOLTAGE_GET:
		pr_info("jingshang TT_VOLTAGE_GET !\n");
		break;

	case TT_CLK_ENABLE:
		pr_info("jingshang TT_CLK_ENABLE !\n");
		break;

	case TT_CLK_DISABLE:
		pr_info("jingshang TT_CLK_DISABLE !\n");
		break;

	case TT_RST_GPIO_SET_VALUE:
		pr_info("jingshang TT_RST_GPIO_SET_VALUE !\n");
		get_user(gpio_value, p);
		if(!(jingshang_data->data))
			break;
		gpio_num = jingshang_data->data[JINGSHANG_GPIO_RST].gpio_num;
		if (gpio_is_valid(gpio_num)) {
			ret = gpio_direction_output(gpio_num, gpio_value);
			if (ret) {
				pr_info("jingshang %s: fail to set gpio %d !\n", __func__, gpio_num);
			}
		} else {
			pr_info("jingshang %s: unable to get gpio %d!\n", __func__, gpio_num);
		}
		break;

	case TT_AP2CP_GPIO_SET_VALUE:
		pr_info("jingshang TT_AP2CP_GPIO_SET_VALUE !\n");
		get_user(gpio_value, p);
		if(!(jingshang_data->data))
			break;
		gpio_num = jingshang_data->data[JINGSHANG_GPIO_AP_SLEEP_BP].gpio_num;
		if (gpio_is_valid(gpio_num)) {
			ret = gpio_direction_output(gpio_num, gpio_value);
			if (ret) {
				pr_info("jingshang %s: fail to set gpio %d !\n", __func__, gpio_num);
			}
		} else {
			pr_info("jingshang %s: unable to get gpio %d!\n", __func__, gpio_num);
		}
		break;

	case TT_STATE_GET:
		if (0 == copy_from_user(&ldo_data, argp, sizeof(ldo_data))) {
			if(TT_SOC_VDD == ldo_data.ldo_type) {
				if((jingshang_data->vdd_1p1) && regulator_is_enabled(jingshang_data->vdd_1p1)) {
					ldo_data.state = 1;
				} else {
					ldo_data.state = 0;
				}
			} else if(TT_SOC_IOVDD == ldo_data.ldo_type) {
				gpio_num = jingshang_data->data[JINGSHANG_GPIO_SAT_1P8_EN].gpio_num;
				if (gpio_is_valid(gpio_num)) {
					gpio_value = gpio_get_value(gpio_num);
					ldo_data.state = gpio_value;
				} else {
					pr_info("jingshang %s: unable to read gpio %d!\n", __func__, gpio_num);
				}
			} else if(TT_RFIC_VCC == ldo_data.ldo_type) {
				gpio_num = jingshang_data->data[JINGSHANG_GPIO_SAT_1P8_EN].gpio_num;
				if (gpio_is_valid(gpio_num)) {
					gpio_value = gpio_get_value(gpio_num);
					ldo_data.state = gpio_value;
				} else {
					pr_info("jingshang %s: unable to read gpio %d!\n", __func__, gpio_num);
				}
			} else if(TT_RFIC_AVCC == ldo_data.ldo_type) {
				if((jingshang_data->vdd_3p3) && regulator_is_enabled(jingshang_data->vdd_3p3)) {
					ldo_data.state = 1;
				} else {
					ldo_data.state = 0;
				}
			} else {
				ret = -EFAULT;
			}
			pr_info("jingshang %d: TT_STATE_GET %d !\n", ldo_data.ldo_type, ldo_data.state);

			if(!ret && (0 == copy_to_user((void __user *)ioctl_param, &ldo_data, sizeof(ldo_data))))
			{
				pr_info("jingshang TT_STATE_GET succ !\n");
			} else {
				pr_info("jingshang TT_STATE_GET failed !\n");
				ret = -EFAULT;
			}

		} else {
			ret = -EFAULT;
		}
		break;

	case TT_CP2AP_GPIO_GET_VALUE:
		pr_info("jingshang TT_CP2AP_GPIO_GET_VALUE !\n");
		if(!(jingshang_data->data))
			break;
		gpio_num = jingshang_data->data[JINGSHANG_GPIO_BP_SLEEP_AP].gpio_num;
		if (gpio_is_valid(gpio_num)) {
			gpio_value = gpio_get_value(gpio_num);
			put_user(gpio_value, p);
		} else {
			pr_info("jingshang %s: unable to read gpio %d!\n", __func__, gpio_num);
		}
		break;

	case TT_TT_UART_EN_SET_VALUE:
		pr_info("jingshang TT_TT_UART_EN_SET_VALUE !\n");
		get_user(gpio_value, p);
		if(!(jingshang_data->data))
			break;
		gpio_num = jingshang_data->data[JINGSHANG_GPIO_TT_UART_EN].gpio_num;
		if (gpio_is_valid(gpio_num)) {
			ret = gpio_direction_output(gpio_num, gpio_value);
			if (ret) {
				pr_info("jingshang %s: fail to set gpio %d !\n", __func__, gpio_num);
			}
		} else {
			pr_info("jingshang %s: unable to get gpio %d!\n", __func__, gpio_num);
		}
		break;

	default:
		pr_err("%s Entered default. Invalid ioctl num %u",
			__func__, ioctl_num);
		ret = -EINVAL;
		break;
	}
	return ret;
}


static const struct file_operations jingshang_gpio_fops = {
	.owner = THIS_MODULE,
	.open = jingshang_gpio_open,
	.release = jingshang_gpio_release,
	.unlocked_ioctl = jingshang_gpio_ioctl,
};


static int gpio_jingshang_reg_chrdev(struct jingshang_gpio_data * jingshang_data)
{
	int ret = 0;

	jingshang_data->driver_name = "tt_chip";
	ret = alloc_chrdev_region(&jingshang_data->jingshang_major, 0,
				MINOR_NUMBER_COUNT, jingshang_data->driver_name);
	if (ret < 0) {
		pr_err("%s alloc_chr_dev_region failed ret : %d\n",
			__func__, ret);
		return ret;
	}
	pr_info("%s major number %d", __func__, MAJOR(jingshang_data->jingshang_major));

	jingshang_data->jingshang_class = class_create(THIS_MODULE,
					jingshang_data->driver_name);
	if (IS_ERR(jingshang_data->jingshang_class)) {
		ret = PTR_ERR(jingshang_data->jingshang_class);
		pr_err("%s class create failed. ret : %d", __func__, ret);
		goto err_class;
	}

	jingshang_data->chardev = device_create(jingshang_data->jingshang_class, NULL,
				jingshang_data->jingshang_major, NULL,
				jingshang_data->driver_name);
	if (IS_ERR(jingshang_data->chardev)) {
		ret = PTR_ERR(jingshang_data->chardev);
		pr_err("%s device create failed ret : %d\n", __func__, ret);
		goto err_device;
	}

	cdev_init(&jingshang_data->cdev, &jingshang_gpio_fops);
	ret = cdev_add(&jingshang_data->cdev, jingshang_data->jingshang_major, 1);
	if (ret) {
		pr_err("%s cdev add failed, ret : %d\n", __func__, ret);
		goto err_cdev;
	}
	return ret;

err_cdev:
	device_destroy(jingshang_data->jingshang_class, jingshang_data->jingshang_major);
err_device:
	class_destroy(jingshang_data->jingshang_class);
err_class:
	unregister_chrdev_region(0, MINOR_NUMBER_COUNT);
	return ret;
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

	ret = gpio_jingshang_reg_chrdev(jingshang_data);
	if (ret) {
		pr_err("%s register char dev failed, rc : %d", __func__, ret);
		return ret;
	}

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

	rc = gpio_direction_output(gpio, 0);
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


