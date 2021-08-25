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
#include <uapi/misc/drv8846.h>


#define DRV8846_DEV_NAME "ti,drv8846"

#define PWM_PERIOD_DEFAULT_NS		1000000
#define RAMP_PERIOD_DEFAULT_NS		625000
#define RAMP_DURATION_DEFAULT_MS	50
#define HIGH_PERIOD_DEFAULT_NS 		52083
#define HIGH_DURATION_DEFAULT_MS	700
#define DEFAULT_STEP_MODE			2

static DECLARE_WAIT_QUEUE_HEAD(poll_wait_queue);

struct pwm_setting {
	u64 pre_period_ns;
	u64 period_ns;
	u64 duty_ns;
};

struct drv8846_soc_ctrl {
	struct platform_device *pdev;
	struct miscdevice miscdev;
	struct dentry *debugfs;
	struct device_node *of_node;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_default;

	struct pwm_device	*pwm_dev;
	struct pwm_setting	pwm_setting;
	struct hrtimer		pwm_timer;
	struct work_struct	pwm_apply_work;

	atomic_t move_done;

	struct mutex motor_mutex;
	struct fasync_struct *async;

	uint32_t rampup_period_ns;
	uint32_t high_period_ns;
	uint32_t rampdown_period_ns;
	uint32_t high_duration_ms;
	uint32_t rampup_duration_ms;
	uint32_t rampdown_duration_ms;
	uint32_t step_mode;
	uint32_t direction;

	int gpio_mode0;
	int gpio_mode1;
	int gpio_dir;
	int gpio_sleep;
	int gpio_pwren;

	enum running_state	state;
};

static int __drv8846_config_pwm(struct drv8846_soc_ctrl *mctrl,
				struct pwm_setting *pwm)
{
	int rc;
	struct pwm_state pstate;

	pwm_get_state(mctrl->pwm_dev, &pstate);
	pstate.enabled = !!(pwm->duty_ns != 0);
	pstate.period = pwm->period_ns;
	pstate.duty_cycle = pwm->duty_ns;
	pstate.output_type = PWM_OUTPUT_FIXED;
	pstate.output_pattern = NULL;
	pr_debug("enable %d\n", pstate.enabled);
	rc = pwm_apply_state(mctrl->pwm_dev, &pstate);
	if (rc < 0)
		pr_err("Apply PWM state failed, rc=%d\n", rc);

	if (pstate.enabled == false) {
		gpio_direction_output(mctrl->gpio_sleep, 0);
		kill_fasync(&mctrl->async, SIGIO, POLL_IN);
		atomic_set(&mctrl->move_done, 1);
		wake_up(&poll_wait_queue);
	} else {
		gpio_direction_output(mctrl->gpio_sleep, 1);
	}
	return rc;
}

static void pwm_config_work(struct work_struct *work)
{
	struct drv8846_soc_ctrl *mctrl = container_of(work, struct drv8846_soc_ctrl, pwm_apply_work);
	struct pwm_setting setting;

	setting = mctrl->pwm_setting;

	__drv8846_config_pwm(mctrl, &setting);
}

static enum hrtimer_restart pwm_hrtimer_handler(struct hrtimer *timer)
{
	uint32_t duration_ms = 0;
	struct drv8846_soc_ctrl *mctrl = container_of(timer, struct drv8846_soc_ctrl, pwm_timer);

	switch (mctrl->state) {
	case SPEEDUP:
		mctrl->state = FULLSTEAM;
		duration_ms = mctrl->high_duration_ms;
		/* reconfig the pwm and timer to fast running mode. */
		mctrl->pwm_setting.period_ns = mctrl->high_period_ns;
		mctrl->pwm_setting.pre_period_ns = mctrl->high_period_ns;
		mctrl->pwm_setting.duty_ns = mctrl->high_period_ns >> 1;
		if (mctrl->direction == DOWN)
			duration_ms += DELTAMS;
		hrtimer_forward_now(&mctrl->pwm_timer,
			ktime_set(duration_ms / MSEC_PER_SEC,
			(duration_ms % MSEC_PER_SEC) * NSEC_PER_MSEC));
		break;
	case FULLSTEAM:
		mctrl->state = SLOWDOWN;
		duration_ms = mctrl->rampdown_duration_ms;
		/* reconfig the pwm and timer to slow mode(brake down) */
		mctrl->pwm_setting.period_ns = mctrl->rampdown_period_ns;
		mctrl->pwm_setting.pre_period_ns = mctrl->rampdown_period_ns;
		mctrl->pwm_setting.duty_ns = mctrl->rampdown_period_ns >> 1;
		hrtimer_forward_now(&mctrl->pwm_timer,
			ktime_set(duration_ms / MSEC_PER_SEC,
			(duration_ms % MSEC_PER_SEC) * NSEC_PER_MSEC));
		break;
	case SLOWDOWN:
	case UNIFORMSPEED:
		mctrl->state = STILL;
		/* should stop timer and pwm. */
		mctrl->pwm_setting.duty_ns = 0;
		break;
	default:
		mctrl->pwm_setting.duty_ns = 0;
		pr_info("default state.");
	}
	schedule_work(&mctrl->pwm_apply_work);

	if(mctrl->state == STILL)
		return HRTIMER_NORESTART;
	else
		return HRTIMER_RESTART;
}

void drv8846_move(struct drv8846_soc_ctrl *mctrl, uint8_t dir)
{
	pr_debug("move %s\n", dir ? "up" : "down");

	hrtimer_start(&mctrl->pwm_timer,
			ktime_set(mctrl->rampup_duration_ms / MSEC_PER_SEC,
			(mctrl->rampup_duration_ms % MSEC_PER_SEC) * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	mctrl->state = SPEEDUP;
	mctrl->direction = dir;

	gpio_direction_output(mctrl->gpio_dir, (dir == UP) ? 0 : 1);
	gpio_direction_output(mctrl->gpio_sleep, 1);

	mctrl->pwm_setting.period_ns = mctrl->rampup_period_ns;
	mctrl->pwm_setting.pre_period_ns = mctrl->rampup_period_ns;
	mctrl->pwm_setting.duty_ns = mctrl->rampup_period_ns >> 1;
	schedule_work(&mctrl->pwm_apply_work);
}

static int drv8846_fasync(int fd, struct file *filp, int mode)
{
	struct drv8846_soc_ctrl *mctrl = filp->private_data;

	pr_debug("mctrl %p, application  fasync!", mctrl);
	return fasync_helper(fd, filp, mode, &mctrl->async);
}

static unsigned int drv8846_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	struct drv8846_soc_ctrl *mctrl = filp->private_data;

	pr_debug("Poll enter\n");

	poll_wait(filp, &poll_wait_queue, wait);
	if (atomic_read(&mctrl->move_done)) {
		atomic_set(&mctrl->move_done, 0);
		mask = POLLIN | POLLRDNORM;
	}

	return mask;
}

static long drv8846_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	uint8_t direction = 0;
	ktime_t time_rem;
	long time_ms = 0;
	struct op_parameter parameters;
	struct drv8846_soc_ctrl *mctrl = filp->private_data;

	pr_debug("enter, mctrl %p", mctrl);

	if (mctrl == NULL)
		return -EFAULT;

	switch (cmd) {
	case MOTOR_IOC_SET_AUTORUN:
		pr_debug("MOTOR_IOCAUTORUN.");
		if (copy_from_user(&direction, (uint8_t *)arg, sizeof(uint8_t))) {
			pr_err("Failed to copy direction from user to kernel\n");
			rc = -EFAULT;
			break;
		}
		drv8846_move(mctrl, direction);
		break;
	case MOTOR_IOC_SET_MANUALRUN:
		pr_debug("MOTOR_IOCMANUAL\n");
		if (copy_from_user(&parameters, (struct op_parameter *)arg, sizeof(struct op_parameter))) {
			pr_err("Failed to copy target position from user to kernel\n");
			rc = -EFAULT;
			break;
		}
		pr_debug("dir %d, duration %dms, period %dns.", parameters.dir, parameters.duration_ms, parameters.period_ns);

		/* configure pwm */
		gpio_direction_output(mctrl->gpio_dir, (parameters.dir == UP) ? 0 : 1);
		gpio_direction_output(mctrl->gpio_sleep, 1);

		mctrl->pwm_setting.period_ns = parameters.period_ns;
		mctrl->pwm_setting.pre_period_ns = parameters.period_ns;
		mctrl->pwm_setting.duty_ns = parameters.period_ns >> 1;
		schedule_work(&mctrl->pwm_apply_work);
		/* start hrtimer */
		hrtimer_start(&mctrl->pwm_timer,
			ktime_set(parameters.duration_ms / MSEC_PER_SEC,
			(parameters.duration_ms % MSEC_PER_SEC) * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
		mctrl->state = UNIFORMSPEED;
		break;
	case MOTOR_IOC_GET_REMAIN_TIME:
		if (hrtimer_active(&mctrl->pwm_timer)) {
			time_rem = hrtimer_get_remaining(&mctrl->pwm_timer);
			time_ms = (long)ktime_to_ms(time_rem);
		}
		if (copy_to_user((void __user *)arg, &time_ms, sizeof(long))) {
			pr_err("copy_to_user failed.");
			return -EFAULT;
		}
		break;
	case MOTOR_IOC_GET_STATE:
		if (copy_to_user((void __user *)arg, &mctrl->state, sizeof(enum running_state))) {
			pr_err("copy_to_user failed.");
			return -EFAULT;
		}
		break;
	default:
		pr_warn("unsupport cmd:0x%x\n", cmd);
		break;
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
	struct drv8846_soc_ctrl *mctrl = NULL;

	pr_debug("enter\n");

	mctrl = container_of(filp->private_data, struct drv8846_soc_ctrl, miscdev);
	filp->private_data = mctrl;

	atomic_set(&mctrl->move_done, 0);

	pr_debug("mctrl: %p\n", mctrl);

	return 0;
}

static int drv8846_release(struct inode *inode, struct file *file)
{
	struct drv8846_soc_ctrl *mctrl = file->private_data;

	atomic_set(&mctrl->move_done, 0);

	pr_debug("mctrl %p.", mctrl);

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
	.fasync = drv8846_fasync,
	.poll = drv8846_poll,
};

static int drv8846_pinctrl_init(struct drv8846_soc_ctrl *mctrl)
{
	int rc = 0;
	/* Get pinctrl if target uses pinctrl */
	mctrl->pinctrl = devm_pinctrl_get(&mctrl->pdev->dev);

	if (IS_ERR_OR_NULL(mctrl->pinctrl)) {
		rc = PTR_ERR(mctrl->pinctrl);
		pr_err("Target does not use pinctrl %d\n", rc);
		goto err_pinctrl_get;
	}

	mctrl->pinctrl_default
		= pinctrl_lookup_state(mctrl->pinctrl, "default");

	if (IS_ERR_OR_NULL(mctrl->pinctrl_default)) {
		rc = PTR_ERR(mctrl->pinctrl_default);
		pr_err("Can not lookup default pinstate %d\n", rc);
		goto err_pinctrl_lookup;
	}

	return 0;

err_pinctrl_lookup:
	devm_pinctrl_put(mctrl->pinctrl);
err_pinctrl_get:
	mctrl->pinctrl = NULL;

	return rc;
}

static int drv8846_gpio_config(struct drv8846_soc_ctrl *mctrl)
{
	int32_t rc = 0;

	rc = gpio_request_one(mctrl->gpio_mode0, GPIOF_OUT_INIT_HIGH, "motor-mode0");
	if (rc < 0) {
		pr_err("Failed to request mode0 GPIO %d", mctrl->gpio_mode0);
		goto fail0;
	}
	gpio_direction_output(mctrl->gpio_mode0, (mctrl->step_mode & 0x01));

	rc = gpio_request_one(mctrl->gpio_mode1, GPIOF_OUT_INIT_HIGH, "motor-mode1");
	if( rc < 0) {
		pr_err("Failed to request mode1 GPIO %d", mctrl->gpio_mode1);
		goto fail1;
	}
	gpio_direction_output(mctrl->gpio_mode1, (mctrl->step_mode & 0x02));

	rc = gpio_request_one(mctrl->gpio_dir, GPIOF_OUT_INIT_HIGH, "motor-dir");
	if (rc < 0) {
		pr_err("Failed to request dir GPIO %d\n", mctrl->gpio_dir);
		goto fail2;
	}
	gpio_direction_output(mctrl->gpio_dir, 0);

	rc = gpio_request_one(mctrl->gpio_sleep, GPIOF_OUT_INIT_HIGH, "motor-sleep");
	if (rc < 0) {
		pr_err("Failed to request sleep GPIO %d", mctrl->gpio_sleep);
		goto fail3;
	}
	gpio_direction_output(mctrl->gpio_sleep, 0);

	rc = gpio_request_one(mctrl->gpio_pwren, GPIOF_OUT_INIT_HIGH, "motor-pwr");
	if (rc < 0) {
		pr_err("Failed to request power enable GPIO %d", mctrl->gpio_pwren);
		goto fail4;
	}
	gpio_direction_output(mctrl->gpio_pwren, 1);

	return 0;

fail4:
	if (gpio_is_valid(mctrl->gpio_sleep))
		gpio_free(mctrl->gpio_sleep);
fail3:
	if (gpio_is_valid(mctrl->gpio_dir))
		gpio_free(mctrl->gpio_dir);
fail2:
	if (gpio_is_valid(mctrl->gpio_mode1))
		gpio_free(mctrl->gpio_mode1);
fail1:
	if (gpio_is_valid(mctrl->gpio_mode0))
		gpio_free(mctrl->gpio_mode0);
fail0:
	return rc;
}

int drv8846_parse_dt(struct drv8846_soc_ctrl *mctrl)
{
	int rc = 0;
	struct device_node *of_node = NULL;

	of_node = mctrl->pdev->dev.of_node;

	mctrl->pwm_dev = devm_of_pwm_get(&mctrl->pdev->dev, of_node, NULL);
	if (IS_ERR(mctrl->pwm_dev)) {
		rc = PTR_ERR(mctrl->pwm_dev);
		if (rc != -EPROBE_DEFER)
			pr_err("Get pwm device for motor failed, rc=%d\n", rc);
		return rc;
	}

	mctrl->pwm_setting.duty_ns = 0;
	mctrl->pwm_setting.period_ns = PWM_PERIOD_DEFAULT_NS;

	mctrl->gpio_mode0 = of_get_named_gpio_flags(of_node, "motor,gpio-mode0", 0, NULL);
	if (!gpio_is_valid(mctrl->gpio_mode0)) {
		pr_info("mctrl->motor_data.gpio_mode0 is invalid.");
		return -EINVAL;
	}

	mctrl->gpio_mode1 = of_get_named_gpio_flags(of_node, "motor,gpio-mode1", 0, NULL);
	if (!gpio_is_valid(mctrl->gpio_mode1)) {
		pr_info("motor,mode1-gpio is invalid.");
		return -EINVAL;
	}

	mctrl->gpio_sleep = of_get_named_gpio_flags(of_node, "motor,gpio-sleep", 0, NULL);
	if (!gpio_is_valid(mctrl->gpio_sleep)) {
		pr_info("motor,sleep-gpio is invalid.");
		return -EINVAL;
	}

	mctrl->gpio_dir = of_get_named_gpio_flags(of_node, "motor,gpio-dir", 0, NULL);
	if (!gpio_is_valid(mctrl->gpio_dir)) {
		pr_info("motor,dir-gpio is invalid.");
		return -EINVAL;
	}

	mctrl->gpio_pwren = of_get_named_gpio_flags(of_node, "motor,gpio-pwren", 0, NULL);
	if (!gpio_is_valid(mctrl->gpio_pwren)) {
		pr_info("power,en-gpio is invalid.");
		return -EINVAL;
	}

	rc = of_property_read_u32(of_node, "motor,rampup-pwm-period-ns", &mctrl->rampup_period_ns);
	if (rc < 0) {
		pr_info("motor,rampup-pwm-period-ns not set, use default.");
		mctrl->rampup_period_ns = RAMP_PERIOD_DEFAULT_NS;
	}

	rc = of_property_read_u32(of_node, "motor,high-pwm-period-ns", &mctrl->high_period_ns);
	if (rc < 0) {
		pr_info("motor,high-pwm-period-ns not set, use default.");
		mctrl->high_period_ns = HIGH_PERIOD_DEFAULT_NS;
	}

	rc = of_property_read_u32(of_node, "motor,rampdown-pwm-period-ns", &mctrl->rampdown_period_ns);
	if (rc < 0) {
		pr_info("motor,rampdown-pwm-period-ns not set, use default.");
		mctrl->rampdown_period_ns = RAMP_PERIOD_DEFAULT_NS;
	}

	rc = of_property_read_u32(of_node, "motor,rampup-duration-ms", &mctrl->rampup_duration_ms);
	if (rc < 0) {
		pr_info("motor,rampup-duration-ms not set, use default.");
		mctrl->rampup_duration_ms = RAMP_DURATION_DEFAULT_MS;
	}

	rc = of_property_read_u32(of_node, "motor,high-duration-ms", &mctrl->high_duration_ms);
	if (rc < 0) {
		pr_info("motor,high-duration-ms not set, use default.");
		mctrl->high_duration_ms = HIGH_DURATION_DEFAULT_MS;
	}

	rc = of_property_read_u32(of_node, "motor,rampdown-duration-ms", &mctrl->rampdown_duration_ms);
	if (rc < 0) {
		pr_info("motor,rampdown-duration-ms not set, use default.");
		mctrl->rampdown_duration_ms = RAMP_DURATION_DEFAULT_MS;
	}

	rc = of_property_read_u32(of_node, "motor,step-mode", &mctrl->step_mode);
	if (rc < 0) {
		pr_info("motor,step-mode not set, use default.");
		mctrl->step_mode = DEFAULT_STEP_MODE;
	}

	return 0;
}

static int step_mode_dbgfs_read(void *data, u64 *val)
{
	struct drv8846_soc_ctrl *mctrl = (struct drv8846_soc_ctrl *)data;

	*val = mctrl->step_mode;

	return 0;
}

static int step_mode_dbgfs_write(void *data, u64 val)
{
	struct drv8846_soc_ctrl *mctrl = (struct drv8846_soc_ctrl *)data;

	mctrl->step_mode = val & 0x03;
	gpio_direction_output(mctrl->gpio_mode0, (val & 0x01));
	gpio_direction_output(mctrl->gpio_mode1, (val & 0x02));

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(step_mode_debugfs_ops,	step_mode_dbgfs_read,
		step_mode_dbgfs_write, "%llu\n");

static int drv8846_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct drv8846_soc_ctrl *mctrl = NULL;

	pr_debug("Enter");
	if (!pdev->dev.of_node) {
		pr_err("of_node NULL");
		return -EINVAL;
	}

	mctrl = kzalloc(sizeof(struct drv8846_soc_ctrl), GFP_KERNEL);
	if (!mctrl)
		return -ENOMEM;

	pr_debug("mctrl: %p\n", mctrl);
	mctrl->pdev = pdev;
	platform_set_drvdata(pdev, mctrl);

	mutex_init(&mctrl->motor_mutex);

	rc = drv8846_parse_dt(mctrl);
	if (rc < 0) {
		pr_err("parse dt failed");
		goto fail;
	}

	rc = drv8846_pinctrl_init(mctrl);
	if (!rc && mctrl->pinctrl) {
		rc = pinctrl_select_state(mctrl->pinctrl, mctrl->pinctrl_default);
		if (rc < 0) {
			pr_err("Failed to select default pinstate %d\n", rc);
		}
	} else {
		pr_err("Failed to init pinctrl\n");
	}

	rc = drv8846_gpio_config(mctrl);
	if (rc < 0) {
		pr_err("Failed to config gpio\n");
		goto fail;
	}

	INIT_WORK(&mctrl->pwm_apply_work, pwm_config_work);

	hrtimer_init(&mctrl->pwm_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	mctrl->pwm_timer.function = pwm_hrtimer_handler;

	mctrl->miscdev.minor = MISC_DYNAMIC_MINOR;
	mctrl->miscdev.name	= DRV8846_MISC_NAME;
	mctrl->miscdev.fops	= &drv8846_fops;
	mctrl->miscdev.parent = &pdev->dev;

	rc = misc_register(&mctrl->miscdev);
	if (rc < 0) {
		pr_err("register misc device fail");
		goto register_fail;
	}

	mctrl->debugfs = debugfs_create_dir("pwm_motor", NULL);
	if (mctrl->debugfs) {
		debugfs_create_u32("rampup_period_ns", 0666, mctrl->debugfs,
			   &mctrl->rampup_period_ns);
		debugfs_create_u32("high_period_ns", 0666, mctrl->debugfs,
			   &mctrl->high_period_ns);
		debugfs_create_u32("rampdown_period_ns", 0666, mctrl->debugfs,
			   &mctrl->rampdown_period_ns);
		debugfs_create_u32("rampup_duration_ms", 0666, mctrl->debugfs,
			   &mctrl->rampup_duration_ms);
		debugfs_create_u32("high_duration_ms", 0666, mctrl->debugfs,
			   &mctrl->high_duration_ms);
		debugfs_create_u32("rampdown_duration_ms", 0666, mctrl->debugfs,
			   &mctrl->rampdown_duration_ms);
		debugfs_create_file("step_mode", 0666, mctrl->debugfs, mctrl,
			    &step_mode_debugfs_ops);
	}

	pr_debug("successfully probed.");
	return 0;

register_fail:
	cancel_work_sync(&mctrl->pwm_apply_work);
	hrtimer_cancel(&mctrl->pwm_timer);
fail:
	mutex_destroy(&mctrl->motor_mutex);
	kfree(mctrl);
	platform_set_drvdata(pdev, NULL);
	return rc;
}

static int drv8846_remove(struct platform_device *pdev)
{
	struct drv8846_soc_ctrl *mctrl = platform_get_drvdata(pdev);

	cancel_work_sync(&mctrl->pwm_apply_work);
	hrtimer_cancel(&mctrl->pwm_timer);
	mutex_destroy(&mctrl->motor_mutex);
	misc_deregister(&mctrl->miscdev);
	platform_set_drvdata(pdev, NULL);
	kfree(mctrl);

	return 0;
}

static int drv8846_suspend(struct device *dev)
{
	struct drv8846_soc_ctrl *mctrl = dev_get_drvdata(dev);

	return gpio_direction_output(mctrl->gpio_pwren, 0);
}

static int drv8846_resume(struct device *dev)
{
	struct drv8846_soc_ctrl *mctrl = dev_get_drvdata(dev);

	return gpio_direction_output(mctrl->gpio_pwren, 1);
}


static const struct dev_pm_ops drv8846_pm_ops = {
	.suspend	= drv8846_suspend,
	.resume		= drv8846_resume,
};


static const struct of_device_id drv8846_match_table[] = {
	{ .compatible = DRV8846_DEV_NAME },
	{}
};

static struct platform_driver drv8846_driver = {
	.driver	= {
		.name		= DRV8846_DEV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= drv8846_match_table,
		.pm		= &drv8846_pm_ops,
	},
	.probe	= drv8846_probe,
	.remove = drv8846_remove,
};

static int __init drv8846_init(void)
{
	return platform_driver_register(&drv8846_driver);
}

static void __exit drv8846_exit(void)
{
	return platform_driver_unregister(&drv8846_driver);
}

module_init(drv8846_init);
module_exit(drv8846_exit);

MODULE_AUTHOR("ran fei <ranfei@xiaomi.com>");
MODULE_DESCRIPTION("TI Dual H-Bridge Stepper Motor Driver");
MODULE_LICENSE("GPL");
