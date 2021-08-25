/* drivers/misc/drv8846.c - drv8846 step motor soc driver
 *
 * Copyright (c) 2014-2015, Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define DEBUG
#define pr_fmt(fmt)	"drv8846: %s: %d " fmt, __func__, __LINE__

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/freezer.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/pwm.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/hrtimer.h>
#include <linux/debugfs.h>
#include <linux/drv8846.h>

#define EVENT_FCAM  0x2
extern void lpm_disable_for_dev(bool on, char event_dev);
static DECLARE_WAIT_QUEUE_HEAD(poll_wait_queue);

struct pwm_setting {
	u64 pre_period_ns;
	u64 period_ns;
	u64 duty_ns;
};

struct drv8846_soc_ctrl {
	dev_t dev_num;
	struct cdev c_cdev;
	struct class *chr_class;
	struct device *chr_dev;

	struct platform_device *pdev;
	struct device_node *of_node;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_default;

	struct pwm_device	*pwm_dev;
	struct pwm_setting	pwm_setting;
	struct hrtimer		pwm_timer;
	struct work_struct	pwm_apply_work;

	struct drv8846_private_data pdata;

	atomic_t move_done;

	struct mutex motor_mutex;

	uint32_t step_mode;
	uint32_t direction;

	int gpio_mode0;
	int gpio_mode1;
	int gpio_dir;
	int gpio_sleep;
	int gpio_pwren;

	enum running_state	state;
};

static int __drv8846_config_pwm(struct drv8846_soc_ctrl *c_ctrl,
				struct pwm_setting *pwm)
{
	int rc;
	struct pwm_state pstate;

	pwm_get_state(c_ctrl->pwm_dev, &pstate);
	pstate.enabled = !!(pwm->duty_ns != 0);
	pstate.period = pwm->period_ns;
	pstate.duty_cycle = pwm->duty_ns;
	pstate.output_type = PWM_OUTPUT_FIXED;
	pstate.output_pattern = NULL;
	pr_debug("enable %d\n", pstate.enabled);
	rc = pwm_apply_state(c_ctrl->pwm_dev, &pstate);
	if (rc < 0)
		pr_err("Apply PWM state failed, rc=%d\n", rc);

	if (pstate.enabled == false) {
		gpio_direction_output(c_ctrl->gpio_sleep, 0);
		atomic_set(&c_ctrl->move_done, 1);
		wake_up(&poll_wait_queue);
		lpm_disable_for_dev(false, EVENT_FCAM);
	} else {
		gpio_direction_output(c_ctrl->gpio_sleep, 1);
	}
	return rc;
}

static void pwm_config_work(struct work_struct *work)
{
	struct drv8846_soc_ctrl *c_ctrl = container_of(work, struct drv8846_soc_ctrl, pwm_apply_work);
	struct pwm_setting setting;

	setting = c_ctrl->pwm_setting;

	__drv8846_config_pwm(c_ctrl, &setting);
}

static enum hrtimer_restart pwm_hrtimer_handler(struct hrtimer *timer)
{
	struct drv8846_soc_ctrl *c_ctrl = container_of(timer, struct drv8846_soc_ctrl, pwm_timer);

	switch (c_ctrl->state) {
	case SPEEDUP: {
		c_ctrl->state = SLOWDOWN;
		c_ctrl->pwm_setting.period_ns = c_ctrl->pdata.slow_period;
		c_ctrl->pwm_setting.pre_period_ns = c_ctrl->pdata.slow_period;
		c_ctrl->pwm_setting.duty_ns = c_ctrl->pdata.slow_period >> 1;
		hrtimer_forward_now(&c_ctrl->pwm_timer,
			ktime_set(c_ctrl->pdata.slow_duration / MSEC_PER_SEC,
			(c_ctrl->pdata.slow_duration % MSEC_PER_SEC) * NSEC_PER_MSEC));
		break;
		}
	case SLOWDOWN:
	case STILL:
	default: {
		c_ctrl->state = STOP;
		c_ctrl->pwm_setting.duty_ns = DUTY_DEFAULT;
		break;
		}
	}
	schedule_work(&c_ctrl->pwm_apply_work);

	if (c_ctrl->state == STOP)
		return HRTIMER_NORESTART;
	else
		return HRTIMER_RESTART;
}

void drv8846_move(struct drv8846_soc_ctrl *c_ctrl)
{
	pr_info("move %s\n", (c_ctrl->pdata.dir ? "up" : "down"));

	c_ctrl->direction = c_ctrl->pdata.dir;

	gpio_direction_output(c_ctrl->gpio_dir, ((c_ctrl->pdata.dir == UP) ? 0 : 1));
	gpio_direction_output(c_ctrl->gpio_sleep, 1);

	c_ctrl->pwm_setting.period_ns = c_ctrl->pdata.speed_period;
	c_ctrl->pwm_setting.pre_period_ns = c_ctrl->pdata.speed_period;
	c_ctrl->pwm_setting.duty_ns = c_ctrl->pdata.speed_period >> 1;

	lpm_disable_for_dev(true, EVENT_FCAM);
	hrtimer_start(&c_ctrl->pwm_timer,
			ktime_set(c_ctrl->pdata.speed_duration / MSEC_PER_SEC,
			(c_ctrl->pdata.speed_duration % MSEC_PER_SEC) * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);

	schedule_work(&c_ctrl->pwm_apply_work);

	return;
}


static unsigned int drv8846_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	struct drv8846_soc_ctrl *c_ctrl = filp->private_data;

	pr_debug("Poll enter\n");

	poll_wait(filp, &poll_wait_queue, wait);
	if (atomic_read(&c_ctrl->move_done)) {
		atomic_set(&c_ctrl->move_done, 0);
		mask = POLLIN | POLLRDNORM;
	}

	return mask;
}

static ssize_t drv8846_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct drv8846_soc_ctrl *c_ctrl = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "period:%d, duration:%d\n", c_ctrl->pdata.speed_period, c_ctrl->pdata.speed_duration);
}

static ssize_t drv8846_debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int enable;
	int ret = 0;
	struct drv8846_soc_ctrl *c_ctrl = dev_get_drvdata(dev);

	ret = sscanf(buf, "%d", &enable);
	if (0 == ret)
		pr_err("Input %d\n", enable);

	c_ctrl->pdata.speed_period = 52083;
	c_ctrl->pdata.speed_duration = 550;

	if (99 == enable) {
		c_ctrl->pdata.dir = UP;
	} else {
		c_ctrl->pdata.dir = DOWN;
	}
	c_ctrl->state = STILL;
	drv8846_move(c_ctrl);

	return count;
}
static DEVICE_ATTR(debug, S_IRUGO | S_IWUSR, drv8846_debug_show, drv8846_debug_store);

static ssize_t drv8846_tuning_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct drv8846_soc_ctrl *c_ctrl = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "period:%d, duration:%d, slow_period = %d, slow_duration = %d\n", c_ctrl->pdata.speed_period, c_ctrl->pdata.speed_duration, c_ctrl->pdata.slow_period, c_ctrl->pdata.slow_duration);
}

static ssize_t drv8846_tuning_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int enable;
	int ret = 0;
	struct drv8846_soc_ctrl *c_ctrl = dev_get_drvdata(dev);

	ret = sscanf(buf, "%d %d %d %d %d", &enable, &c_ctrl->pdata.speed_period, &c_ctrl->pdata.speed_duration, &c_ctrl->pdata.slow_period, &c_ctrl->pdata.slow_duration);
	if (0 == ret)
		pr_err("Input %d\n", enable);

	pr_err("enable = %d, speed_period = %d, speed_duration = %d, slow_period = %d, slow_duration = %d\n", enable, c_ctrl->pdata.speed_period, c_ctrl->pdata.speed_duration, c_ctrl->pdata.slow_period, c_ctrl->pdata.slow_duration);

	if (99 == enable) {
		c_ctrl->pdata.dir = UP;
	} else {
		c_ctrl->pdata.dir = DOWN;
	}
	c_ctrl->state = SPEEDUP;
	drv8846_move(c_ctrl);

	return count;
}
static DEVICE_ATTR(tuning, S_IRUGO | S_IWUSR, drv8846_tuning_show, drv8846_tuning_store);

static struct device_attribute *drv8846_attrs[] = {
	&dev_attr_debug,
	&dev_attr_tuning,
};

static long drv8846_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	//ktime_t time_rem;
	struct drv8846_soc_ctrl *c_ctrl = filp->private_data;

	if (NULL == c_ctrl) {
		pr_err("Private data is NULL\n");
		return -EFAULT;
	}

	if (_IOC_TYPE(cmd) != MOTOR_IOC_MAGIC) {
		pr_err("Magic number is worng\n");
		return -ENODEV;
	}

	if (_IOC_DIR(cmd) & _IOC_READ)
		rc = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		rc = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (rc) {
		pr_err("IOCTL cd access failed\n");
		return -EFAULT;
	}

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		memset(&c_ctrl->pdata, 0, sizeof(struct drv8846_private_data));
		if (copy_from_user(&c_ctrl->pdata, (void __user *)arg, sizeof(struct drv8846_private_data))) {
			pr_err("Copy data from user space failed");
			return -EFAULT;
		}
	}

	switch (cmd) {
	case MOTOR_IOC_SET_AUTO:
		c_ctrl->state = SPEEDUP;
		drv8846_move(c_ctrl);
		break;
	case MOTOR_IOC_SET_MANUAL:
		c_ctrl->state = STILL;
		drv8846_move(c_ctrl);
		break;
	case MOTOR_IOC_GET_REMAIN_TIME:
#if 0
		if (hrtimer_active(&c_ctrl->pwm_timer)) {
			time_rem = hrtimer_get_remaining(&c_ctrl->pwm_timer);
			c_ctrl->pwm_time = (long)ktime_to_ms(time_rem);
		}
#endif
		break;
	case MOTOR_IOC_GET_STATE:
		c_ctrl->pdata.pwm_state = c_ctrl->state;
		break;
	default:
		pr_warn("unsupport cmd:0x%x\n", cmd);
		break;
	}

	if (_IOC_DIR(cmd) & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &c_ctrl->pdata, sizeof(struct drv8846_private_data))) {
			pr_err("Copy data to user space failed\n");
			return -EFAULT;
		}
	}

	return rc;
}

#ifdef CONFIG_COMPAT
static long drv8846_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return drv8846_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int drv8846_open(struct inode *inode, struct file *filp)
{
	struct drv8846_soc_ctrl *c_ctrl = NULL;

	c_ctrl = container_of(inode->i_cdev, struct drv8846_soc_ctrl, c_cdev);
	filp->private_data = c_ctrl;

	atomic_set(&c_ctrl->move_done, 0);

	return 0;
}

static int drv8846_release(struct inode *inode, struct file *file)
{
	struct drv8846_soc_ctrl *c_ctrl = file->private_data;

	atomic_set(&c_ctrl->move_done, 0);

	return 0;
}

static const struct file_operations drv8846_fops = {
	.owner =	THIS_MODULE,
	.open = 	drv8846_open,
	.release =	drv8846_release,
	.unlocked_ioctl = drv8846_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = drv8846_compat_ioctl,
#endif
	.poll = drv8846_poll,
};

static int drv8846_pinctrl_init(struct drv8846_soc_ctrl *c_ctrl)
{
	int rc = 0;
	/* Get pinctrl if target uses pinctrl */
	c_ctrl->pinctrl = devm_pinctrl_get(&c_ctrl->pdev->dev);
	if (IS_ERR_OR_NULL(c_ctrl->pinctrl)) {
		rc = PTR_ERR(c_ctrl->pinctrl);
		pr_err("Target does not use pinctrl %d\n", rc);
		goto get_pinctrl_err;
	}

	c_ctrl->pinctrl_default	= pinctrl_lookup_state(c_ctrl->pinctrl, "default");
	if (IS_ERR_OR_NULL(c_ctrl->pinctrl_default)) {
		rc = PTR_ERR(c_ctrl->pinctrl_default);
		pr_err("Can not lookup default pinstate %d\n", rc);
		goto lookup_pinctrl_err;
	}

	return 0;

lookup_pinctrl_err:
	devm_pinctrl_put(c_ctrl->pinctrl);

get_pinctrl_err:
	c_ctrl->pinctrl = NULL;

	return rc;
}

static int drv8846_gpio_config(struct drv8846_soc_ctrl *c_ctrl)
{
	int32_t rc = 0;

	rc = gpio_request_one(c_ctrl->gpio_mode0, GPIOF_OUT_INIT_HIGH, "motor-mode0");
	if (rc < 0) {
		pr_err("Failed to request mode0 GPIO %d\n", c_ctrl->gpio_mode0);
		goto mode0_gpio_req_err;
	}
	gpio_direction_output(c_ctrl->gpio_mode0, (c_ctrl->step_mode & 0x01));

	rc = gpio_request_one(c_ctrl->gpio_mode1, GPIOF_OUT_INIT_HIGH, "motor-mode1");
	if (rc < 0) {
		pr_err("Failed to request mode1 GPIO %d\n", c_ctrl->gpio_mode1);
		goto mode1_gpio_req_err;
	}
	gpio_direction_output(c_ctrl->gpio_mode1, (c_ctrl->step_mode & 0x02));

	rc = gpio_request_one(c_ctrl->gpio_dir, GPIOF_OUT_INIT_HIGH, "motor-dir");
	if (rc < 0) {
		pr_err("Failed to request direction GPIO %d\n", c_ctrl->gpio_dir);
		goto dir_gpio_req_err;
	}
	gpio_direction_output(c_ctrl->gpio_dir, 0);

	rc = gpio_request_one(c_ctrl->gpio_sleep, GPIOF_OUT_INIT_LOW, "motor-sleep");
	if (rc < 0) {
		pr_err("Failed to request sleep GPIO %d\n", c_ctrl->gpio_sleep);
		goto sleep_gpio_req_err;
	}
	gpio_direction_output(c_ctrl->gpio_sleep, 0);

	rc = gpio_request_one(c_ctrl->gpio_pwren, GPIOF_OUT_INIT_HIGH, "motor-pwr");
	if (rc < 0) {
		pr_err("Failed to request power enable GPIO %d\n", c_ctrl->gpio_pwren);
		goto pwren_gpio_req_err;
	}
	gpio_direction_output(c_ctrl->gpio_pwren, 1);

	return 0;

pwren_gpio_req_err:
	if (gpio_is_valid(c_ctrl->gpio_sleep))
		gpio_free(c_ctrl->gpio_sleep);

sleep_gpio_req_err:
	if (gpio_is_valid(c_ctrl->gpio_dir))
		gpio_free(c_ctrl->gpio_dir);

dir_gpio_req_err:
	if (gpio_is_valid(c_ctrl->gpio_mode1)) {
		gpio_direction_output(c_ctrl->gpio_mode1, 0);
		gpio_free(c_ctrl->gpio_mode1);
	}

mode1_gpio_req_err:
	if (gpio_is_valid(c_ctrl->gpio_mode0)) {
		gpio_direction_output(c_ctrl->gpio_mode0, 0);
		gpio_free(c_ctrl->gpio_mode0);
	}

mode0_gpio_req_err:
	return rc;
}

int drv8846_parse_dt(struct drv8846_soc_ctrl *c_ctrl)
{
	int rc = 0;
	struct device_node *of_node = NULL;

	of_node = c_ctrl->pdev->dev.of_node;

	c_ctrl->pwm_setting.duty_ns = DUTY_DEFAULT;
	c_ctrl->pwm_setting.period_ns = PERIOD_DEFAULT;

	c_ctrl->gpio_mode0 = of_get_named_gpio_flags(of_node, "motor,gpio-mode0", 0, NULL);
	if (!gpio_is_valid(c_ctrl->gpio_mode0)) {
		pr_err("Gpio mode0 pin %d is invalid.", c_ctrl->gpio_mode0);
		goto parse_gpio_err;
	}

	c_ctrl->gpio_mode1 = of_get_named_gpio_flags(of_node, "motor,gpio-mode1", 0, NULL);
	if (!gpio_is_valid(c_ctrl->gpio_mode1)) {
		pr_err("Gpio mode1 pin %d is invalid.", c_ctrl->gpio_mode1);
		goto parse_gpio_err;
	}

	c_ctrl->gpio_sleep = of_get_named_gpio_flags(of_node, "motor,gpio-sleep", 0, NULL);
	if (!gpio_is_valid(c_ctrl->gpio_sleep)) {
		pr_err("Gpio sleep pin %d is invalid.", c_ctrl->gpio_sleep);
		goto parse_gpio_err;
	}

	c_ctrl->gpio_dir = of_get_named_gpio_flags(of_node, "motor,gpio-dir", 0, NULL);
	if (!gpio_is_valid(c_ctrl->gpio_dir)) {
		pr_err("Gpio direction pin %d is invalid.", c_ctrl->gpio_dir);
		goto parse_gpio_err;
	}

	c_ctrl->gpio_pwren = of_get_named_gpio_flags(of_node, "motor,gpio-pwren", 0, NULL);
	if (!gpio_is_valid(c_ctrl->gpio_pwren)) {
		pr_err("Gpio power enable pin %d is invalid.", c_ctrl->gpio_pwren);
		goto parse_gpio_err;
	}

	rc = of_property_read_u32(of_node, "motor,step-mode", &c_ctrl->step_mode);
	if (rc < 0) {
		pr_err("Gpio step mode pin %d not set, use default.", DEFAULT_STEP_MODE);
		c_ctrl->step_mode = DEFAULT_STEP_MODE;
	}
	pr_err("gpio-dir = %d\n", c_ctrl->gpio_dir);

	return 0;

parse_gpio_err:

	return -EINVAL;
}

static int drv8846_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct drv8846_soc_ctrl *c_ctrl = NULL;

	pr_info("Enter");

	if (!pdev->dev.of_node) {
		pr_err("of_node NULL");
		return -EINVAL;
	}

	c_ctrl = kzalloc(sizeof(struct drv8846_soc_ctrl), GFP_KERNEL);
	if (!c_ctrl)
		return -ENOMEM;

	c_ctrl->pdev = pdev;
	platform_set_drvdata(pdev, c_ctrl);

	pr_debug("Start parse device tree");
	if (pdev->dev.of_node) {
		rc = drv8846_parse_dt(c_ctrl);
		if (rc < 0) {
			pr_err("Parse dt failed");
			goto parse_dt_err;
		}
	}

	rc = drv8846_pinctrl_init(c_ctrl);
	if (rc) {
		pr_err("Failed to init pinctrl\n");
		goto init_pinctrl_err;
	} else {
		if (c_ctrl->pinctrl) {
			rc = pinctrl_select_state(c_ctrl->pinctrl, c_ctrl->pinctrl_default);
			if (rc < 0) {
				pr_err("Failed to select default pinstate %d\n", rc);
				goto select_pinctrl_err;
			}
		}
	}

	rc = drv8846_gpio_config(c_ctrl);
	if (rc < 0) {
		pr_err("Failed to config gpio\n");
		goto select_pinctrl_err;
	}

	c_ctrl->pwm_dev = devm_of_pwm_get(&c_ctrl->pdev->dev, c_ctrl->pdev->dev.of_node, NULL);
	if (IS_ERR(c_ctrl->pwm_dev)) {
		rc = PTR_ERR(c_ctrl->pwm_dev);
		if (rc != -EPROBE_DEFER)
			pr_err("Get pwm device for motor failed, rc=%d\n", rc);
		goto get_pwm_device_err;
	}

	mutex_init(&c_ctrl->motor_mutex);

	INIT_WORK(&c_ctrl->pwm_apply_work, pwm_config_work);

	hrtimer_init(&c_ctrl->pwm_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	c_ctrl->pwm_timer.function = pwm_hrtimer_handler;

	c_ctrl->chr_class = class_create(THIS_MODULE, DRV8846_CLASS_NAME);
	if (c_ctrl->chr_class == NULL) {
		pr_err("Failed to create class\n");
		rc = -ENODEV;
		goto create_class_err;
	}

	rc = alloc_chrdev_region(&c_ctrl->dev_num, 0, 1, DRV8846_DRV_NAME);
	if (rc < 0) {
		pr_err("Failed to allocate chrdev region\n");
		goto alloc_dev_err;
	}

	c_ctrl->chr_dev = device_create(c_ctrl->chr_class, NULL, c_ctrl->dev_num, c_ctrl, DRV8846_DRV_NAME);
	if (IS_ERR(c_ctrl->chr_dev)) {
		pr_err("Failed to create char device\n");
		rc = PTR_ERR(c_ctrl->chr_dev);
		goto create_device_err;
	}

	cdev_init(&(c_ctrl->c_cdev), &drv8846_fops);
	c_ctrl->c_cdev.owner = THIS_MODULE;

	rc = cdev_add(&(c_ctrl->c_cdev), c_ctrl->dev_num, 1);
	if (rc < 0) {
		pr_err("Failed to add cdev\n");
		goto add_cdev_err;
	}

	rc = device_create_file(c_ctrl->chr_dev, drv8846_attrs[0]);
	if (rc < 0)
		pr_err("Failed to create debug file: %d\n", rc);

	rc = device_create_file(c_ctrl->chr_dev, drv8846_attrs[1]);
	if (rc < 0)
		pr_err("Failed to create debug file: %d\n", rc);

	pr_debug("Successfully probed\n");

	return 0;

add_cdev_err:
	if (c_ctrl->chr_dev)
		device_destroy(c_ctrl->chr_class, c_ctrl->dev_num);

create_device_err:
	unregister_chrdev_region(c_ctrl->dev_num, 1);

alloc_dev_err:
	if (c_ctrl->chr_class)
		class_destroy(c_ctrl->chr_class);

create_class_err:
	cancel_work_sync(&c_ctrl->pwm_apply_work);
	hrtimer_cancel(&c_ctrl->pwm_timer);
	mutex_destroy(&c_ctrl->motor_mutex);
	devm_pwm_put(&c_ctrl->pdev->dev, c_ctrl->pwm_dev);

get_pwm_device_err:
	if (gpio_is_valid(c_ctrl->gpio_pwren)) {
		gpio_direction_output(c_ctrl->gpio_pwren, 0);
		gpio_free(c_ctrl->gpio_pwren);
	}
	if (gpio_is_valid(c_ctrl->gpio_sleep))
		gpio_free(c_ctrl->gpio_sleep);       
	if (gpio_is_valid(c_ctrl->gpio_dir)) 
		gpio_free(c_ctrl->gpio_dir);
	if (gpio_is_valid(c_ctrl->gpio_mode1)) {
		gpio_direction_output(c_ctrl->gpio_mode1, 0);
		gpio_free(c_ctrl->gpio_mode1);
	}
	if (gpio_is_valid(c_ctrl->gpio_mode0)) {
		gpio_direction_output(c_ctrl->gpio_mode0, 0);
		gpio_free(c_ctrl->gpio_mode0);
	}

select_pinctrl_err:
	if (c_ctrl->pinctrl) {
		devm_pinctrl_put(c_ctrl->pinctrl);
		c_ctrl->pinctrl = NULL;
	}

init_pinctrl_err:
parse_dt_err:
	platform_set_drvdata(pdev, NULL);
	kfree(c_ctrl);

	return rc;
}

static int drv8846_remove(struct platform_device *pdev)
{
	struct drv8846_soc_ctrl *c_ctrl = platform_get_drvdata(pdev);

	if (c_ctrl->chr_dev)
		device_destroy(c_ctrl->chr_class, c_ctrl->dev_num);

	unregister_chrdev_region(c_ctrl->dev_num, 1);

	if (c_ctrl->chr_class)
		class_destroy(c_ctrl->chr_class);

	cancel_work_sync(&c_ctrl->pwm_apply_work);
	hrtimer_cancel(&c_ctrl->pwm_timer);
	lpm_disable_for_dev(false, EVENT_FCAM);
	mutex_destroy(&c_ctrl->motor_mutex);
	devm_pwm_put(&c_ctrl->pdev->dev, c_ctrl->pwm_dev);

	if (gpio_is_valid(c_ctrl->gpio_pwren)) {
		gpio_direction_output(c_ctrl->gpio_pwren, 0);
		gpio_free(c_ctrl->gpio_pwren);
	}
	if (gpio_is_valid(c_ctrl->gpio_sleep))
		gpio_free(c_ctrl->gpio_sleep);
	if (gpio_is_valid(c_ctrl->gpio_dir))
		gpio_free(c_ctrl->gpio_dir);
	if (gpio_is_valid(c_ctrl->gpio_mode1)) {
		gpio_direction_output(c_ctrl->gpio_mode1, 0);
		gpio_free(c_ctrl->gpio_mode1);
	}
	if (gpio_is_valid(c_ctrl->gpio_mode0)) {
		gpio_direction_output(c_ctrl->gpio_mode0, 0);
		gpio_free(c_ctrl->gpio_mode0);
	}
	if (c_ctrl->pinctrl) {
		devm_pinctrl_put(c_ctrl->pinctrl);
		c_ctrl->pinctrl = NULL;
	}

	platform_set_drvdata(pdev, NULL);
	kfree(c_ctrl);

	return 0;
}

static int drv8846_suspend(struct device *dev)
{
	struct drv8846_soc_ctrl *c_ctrl = dev_get_drvdata(dev);

	return gpio_direction_output(c_ctrl->gpio_pwren, 0);
}

static int drv8846_resume(struct device *dev)
{
	struct drv8846_soc_ctrl *c_ctrl = dev_get_drvdata(dev);

	return gpio_direction_output(c_ctrl->gpio_pwren, 1);
}


static const struct dev_pm_ops drv8846_pm_ops = {
	.suspend	= drv8846_suspend,
	.resume		= drv8846_resume,
};

static const struct of_device_id drv8846_match_table[] = {
        { .compatible = DRV8846_DEV_NAME },
        {}
};
MODULE_DEVICE_TABLE(of, drv8846_match_table);

static struct platform_driver drv8846_driver = {
        .driver = {
                .name           = DRV8846_DEV_NAME,
                .owner          = THIS_MODULE,
                .of_match_table = of_match_ptr(drv8846_match_table),
                .pm             = &drv8846_pm_ops,
        },
        .probe  = drv8846_probe,
        .remove = drv8846_remove,
};

module_platform_driver(drv8846_driver);

MODULE_DESCRIPTION("TI Dual H-Bridge Stepper Motor Driver");
MODULE_AUTHOR("Zhu Nengjin <zhunengjin@xiaomi.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("2.0");
