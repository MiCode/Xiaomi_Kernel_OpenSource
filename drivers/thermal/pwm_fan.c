/*
 * pwm_fan.c fan driver that is controlled by pwm
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Anshul Jain <anshulj@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/therm_est.h>
#include <linux/slab.h>
#include <linux/platform_data/pwm_fan.h>
#include <linux/thermal.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/pwm.h>
#include <linux/device.h>
#include <linux/sysfs.h>

struct fan_dev_data {
	int next_state;
	int active_steps;
	int *fan_rpm;
	int *fan_pwm;
	int *fan_rru;
	int *fan_rrd;
	struct workqueue_struct *workqueue;
	int fan_temp_control_flag;
	struct pwm_device *pwm_dev;
	int fan_cap_pwm;
	int fan_cur_pwm;
	int next_target_pwm;
	struct thermal_cooling_device *cdev;
	struct delayed_work fan_ramp_work;
	int step_time;
	int precision_multiplier;
	struct mutex fan_state_lock;
	int pwm_period;
	struct device *dev;
};

#ifdef CONFIG_DEBUG_FS
static struct dentry *fan_debugfs_root;

static int fan_target_pwm_show(void *data, u64 *val)
{
	struct fan_dev_data *fan_data = (struct fan_dev_data *)data;

	if (!fan_data)
		return -EINVAL;
	mutex_lock(&fan_data->fan_state_lock);
	*val = ((struct fan_dev_data *)data)->next_target_pwm /
					fan_data->precision_multiplier;
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int fan_target_pwm_set(void *data, u64 val)
{
	struct fan_dev_data *fan_data = (struct fan_dev_data *)data;

	if (!fan_data)
		return -EINVAL;

	if (val > fan_data->pwm_period)
		val = fan_data->pwm_period;

	mutex_lock(&fan_data->fan_state_lock);
	fan_data->next_target_pwm =
		min((int)(val * fan_data->precision_multiplier),
		fan_data->fan_cap_pwm);

	if (fan_data->next_target_pwm != fan_data->fan_cur_pwm)
		queue_delayed_work(fan_data->workqueue,
					&fan_data->fan_ramp_work,
					msecs_to_jiffies(fan_data->step_time));
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int fan_temp_control_show(void *data, u64 *val)
{
	struct fan_dev_data *fan_data = (struct fan_dev_data *)data;

	if (!fan_data)
		return -EINVAL;
	mutex_lock(&fan_data->fan_state_lock);
	*val = fan_data->fan_temp_control_flag;
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int fan_temp_control_set(void *data, u64 val)
{
	struct fan_dev_data *fan_data = (struct fan_dev_data *)data;

	if (!fan_data)
		return -EINVAL;

	mutex_lock(&fan_data->fan_state_lock);
	fan_data->fan_temp_control_flag = val > 0 ? 1 : 0;
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int fan_cap_pwm_set(void *data, u64 val)
{
	struct fan_dev_data *fan_data = (struct fan_dev_data *)data;

	if (!fan_data)
		return -EINVAL;

	if (val > fan_data->pwm_period)
		val = fan_data->pwm_period;
	mutex_lock(&fan_data->fan_state_lock);
	fan_data->fan_cap_pwm = val * fan_data->precision_multiplier;
	fan_data->next_target_pwm = min(fan_data->fan_cap_pwm,
					fan_data->next_target_pwm);
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int fan_cap_pwm_show(void *data, u64 *val)
{
	struct fan_dev_data *fan_data = (struct fan_dev_data *)data;

	if (!fan_data)
		return -EINVAL;
	mutex_lock(&fan_data->fan_state_lock);
	*val = fan_data->fan_cap_pwm / fan_data->precision_multiplier;
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int fan_step_time_set(void *data, u64 val)
{
	struct fan_dev_data *fan_data = (struct fan_dev_data *)data;

	if (!fan_data)
		return -EINVAL;
	mutex_lock(&fan_data->fan_state_lock);
	fan_data->step_time = val;
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int fan_cur_pwm_show(void *data, u64 *val)
{
	struct fan_dev_data *fan_data = (struct fan_dev_data *)data;

	if (!fan_data)
		return -EINVAL;
	mutex_lock(&fan_data->fan_state_lock);
	*val = (fan_data->fan_cur_pwm / fan_data->precision_multiplier);
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int fan_step_time_show(void *data, u64 *val)
{
	struct fan_dev_data *fan_data = (struct fan_dev_data *)data;

	if (!fan_data)
		return -EINVAL;
	mutex_lock(&fan_data->fan_state_lock);
	*val = fan_data->step_time;
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int fan_debugfs_show(struct seq_file *s, void *data)
{
	int i;
	struct fan_dev_data *fan_data = s->private;

	if (!fan_data)
		return -EINVAL;
	seq_printf(s, "(Index, RPM, PWM, RRU*1024, RRD*1024)\n");
	for (i = 0; i < fan_data->active_steps; i++) {
		seq_printf(s, "(%d, %d, %d, %d, %d)\n", i, fan_data->fan_rpm[i],
			fan_data->fan_pwm[i]/fan_data->precision_multiplier,
			fan_data->fan_rru[i],
			fan_data->fan_rrd[i]);
	}
	return 0;
}

static int fan_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, fan_debugfs_show, inode->i_private);
}

static const struct file_operations fan_rpm_table_fops = {
	.open		= fan_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

DEFINE_SIMPLE_ATTRIBUTE(fan_cap_pwm_fops,
			fan_cap_pwm_show,
			fan_cap_pwm_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fan_temp_control_fops,
			fan_temp_control_show,
			fan_temp_control_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fan_target_pwm_fops,
			fan_target_pwm_show,
			fan_target_pwm_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fan_cur_pwm_fops,
			fan_cur_pwm_show,
			NULL, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fan_step_time_fops,
			fan_step_time_show,
			fan_step_time_set, "%llu\n");

static int pwm_fan_debug_init(struct fan_dev_data *fan_data)
{
	fan_debugfs_root = debugfs_create_dir("tegra_fan", 0);

	if (!fan_debugfs_root)
		return -ENOMEM;

	if (!debugfs_create_file("target_pwm", 0644, fan_debugfs_root,
		(void *)fan_data,
		&fan_target_pwm_fops))
		goto err_out;

	if (!debugfs_create_file("temp_control", 0644, fan_debugfs_root,
		(void *)fan_data,
		&fan_temp_control_fops))
		goto err_out;

	if (!debugfs_create_file("pwm_cap", 0644, fan_debugfs_root,
		(void *)fan_data,
		&fan_cap_pwm_fops))
		goto err_out;

	if (!debugfs_create_file("pwm_rpm_table", 0444, fan_debugfs_root,
		(void *)fan_data,
		&fan_rpm_table_fops))
		goto err_out;

	if (!debugfs_create_file("step_time", 0644, fan_debugfs_root,
		(void *)fan_data,
		&fan_step_time_fops))
		goto err_out;

	if (!debugfs_create_file("cur_pwm", 0444, fan_debugfs_root,
		(void *)fan_data,
		&fan_cur_pwm_fops))
		goto err_out;
	return 0;

err_out:
	debugfs_remove_recursive(fan_debugfs_root);
	return -ENOMEM;
}
#else
static inline int pwm_fan_debug_init(struct fan_dev_data *fan_data)
{
	return 0;
}
#endif /* DEBUG_FS*/

static int pwm_fan_get_cur_state(struct thermal_cooling_device *cdev,
						unsigned long *cur_state)
{
	struct fan_dev_data *fan_data = cdev->devdata;

	if (!fan_data)
		return -EINVAL;

	mutex_lock(&fan_data->fan_state_lock);
	*cur_state = fan_data->next_state;
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int pwm_fan_set_cur_state(struct thermal_cooling_device *cdev,
						unsigned long cur_state)
{
	struct fan_dev_data *fan_data = cdev->devdata;

	if (!fan_data)
		return -EINVAL;

	mutex_lock(&fan_data->fan_state_lock);

	fan_data->next_state = cur_state;

	if (fan_data->next_state <= 0)
		fan_data->next_target_pwm = 0;
	else
		fan_data->next_target_pwm = fan_data->fan_pwm[cur_state];

	fan_data->next_target_pwm =
		min(fan_data->fan_cap_pwm, fan_data->next_target_pwm);
	if (fan_data->next_target_pwm != fan_data->fan_cur_pwm &&
		(fan_data->fan_temp_control_flag))
		queue_delayed_work(fan_data->workqueue,
					&(fan_data->fan_ramp_work),
					msecs_to_jiffies(fan_data->step_time));

	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int pwm_fan_get_max_state(struct thermal_cooling_device *cdev,
						unsigned long *max_state)
{
	struct fan_dev_data *fan_data = cdev->devdata;

	*max_state = fan_data->active_steps;
	return 0;
}

static struct thermal_cooling_device_ops pwm_fan_cooling_ops = {
	.get_max_state = pwm_fan_get_max_state,
	.get_cur_state = pwm_fan_get_cur_state,
	.set_cur_state = pwm_fan_set_cur_state,
};

static int fan_get_rru(int pwm, struct fan_dev_data *fan_data)
{
	int i;

	for (i = 0; i < fan_data->active_steps - 1 ; i++) {
		if ((pwm >= fan_data->fan_pwm[i]) &&
				(pwm < fan_data->fan_pwm[i + 1])) {
			return fan_data->fan_rru[i];
		}
	}
	return fan_data->fan_rru[fan_data->active_steps - 1];
}

static int fan_get_rrd(int pwm, struct fan_dev_data *fan_data)
{
	int i;

	for (i = 0; i < fan_data->active_steps - 1 ; i++) {
		if ((pwm >= fan_data->fan_pwm[i]) &&
				(pwm < fan_data->fan_pwm[i + 1])) {
			return fan_data->fan_rrd[i];
		}
	}
	return fan_data->fan_rrd[fan_data->active_steps - 1];
}

static void set_pwm_duty_cycle(int pwm, struct fan_dev_data *fan_data)
{
	if (fan_data != NULL && fan_data->pwm_dev != NULL) {
		pwm_config(fan_data->pwm_dev, fan_data->pwm_period - pwm,
							fan_data->pwm_period);
		pwm_enable(fan_data->pwm_dev);
	} else {
		dev_err(fan_data->dev,
				"FAN:PWM device or fan data is null\n");
	}
}

static int get_next_higher_pwm(int pwm, struct fan_dev_data *fan_data)
{
	int i;

	for (i = 0; i < fan_data->active_steps; i++)
		if (pwm < fan_data->fan_pwm[i])
			return fan_data->fan_pwm[i];

	return fan_data->fan_pwm[fan_data->active_steps - 1];
}

static int get_next_lower_pwm(int pwm, struct fan_dev_data *fan_data)
{
	int i;

	for (i = fan_data->active_steps - 1; i >= 0; i--)
		if (pwm > fan_data->fan_pwm[i])
			return fan_data->fan_pwm[i];

	return fan_data->fan_pwm[fan_data->active_steps - 1];
}

static void fan_ramping_work_func(struct work_struct *work)
{
	int rru, rrd;
	int cur_pwm, next_pwm;
	struct delayed_work *dwork = container_of(work, struct delayed_work,
									work);
	struct fan_dev_data *fan_data = container_of(dwork, struct
						fan_dev_data, fan_ramp_work);

	if (!fan_data) {
		dev_err(fan_data->dev, "Fan data is null\n");
		return;
	}
	mutex_lock(&fan_data->fan_state_lock);
	cur_pwm = fan_data->fan_cur_pwm;
	rru = fan_get_rru(cur_pwm, fan_data);
	rrd = fan_get_rrd(cur_pwm, fan_data);
	next_pwm = cur_pwm;

	if (fan_data->next_target_pwm > fan_data->fan_cur_pwm) {
		fan_data->fan_cur_pwm = fan_data->fan_cur_pwm + rru;
		next_pwm = min(
				get_next_higher_pwm(cur_pwm, fan_data),
				fan_data->fan_cur_pwm);
		next_pwm = min(fan_data->next_target_pwm, next_pwm);
		next_pwm = min(fan_data->fan_cap_pwm, next_pwm);
	} else if (fan_data->next_target_pwm < fan_data->fan_cur_pwm) {
		fan_data->fan_cur_pwm = fan_data->fan_cur_pwm - rrd;
		next_pwm = max(get_next_lower_pwm(cur_pwm, fan_data),
							fan_data->fan_cur_pwm);
		next_pwm = max(next_pwm, fan_data->next_target_pwm);
		next_pwm = max(0, next_pwm);
	}
	set_pwm_duty_cycle(next_pwm/fan_data->precision_multiplier, fan_data);
	fan_data->fan_cur_pwm = next_pwm;
	if (fan_data->next_target_pwm != next_pwm)
		queue_delayed_work(fan_data->workqueue,
				&(fan_data->fan_ramp_work),
				msecs_to_jiffies(fan_data->step_time));
	mutex_unlock(&fan_data->fan_state_lock);
}


static ssize_t show_fan_pwm_cap_sysfs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fan_dev_data *fan_data = dev_get_drvdata(dev);
	int ret;

	if (!fan_data)
		return -EINVAL;
	mutex_lock(&fan_data->fan_state_lock);
	ret = sprintf(buf, "%d\n",
		(fan_data->fan_cap_pwm / fan_data->precision_multiplier));
	mutex_unlock(&fan_data->fan_state_lock);
	return ret;
}

static ssize_t set_fan_pwm_cap_sysfs(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fan_dev_data *fan_data = dev_get_drvdata(dev);
	long val;
	int ret;

	ret = kstrtol(buf, 10, &val);

	if (ret < 0)
		return -EINVAL;

	if (!fan_data)
		return -EINVAL;

	if (val < 0)
		val = 0;
	else if (val > fan_data->pwm_period)
		val = fan_data->pwm_period;
	mutex_lock(&fan_data->fan_state_lock);
	fan_data->fan_cap_pwm = val * fan_data->precision_multiplier;
	fan_data->next_target_pwm = min(fan_data->fan_cap_pwm,
					fan_data->next_target_pwm);
	mutex_unlock(&fan_data->fan_state_lock);
	return count;
}

static DEVICE_ATTR(pwm_cap, S_IWUSR | S_IRUGO, show_fan_pwm_cap_sysfs,
							set_fan_pwm_cap_sysfs);
static struct attribute *pwm_fan_attributes[] = {
	&dev_attr_pwm_cap.attr,
	NULL
};

static const struct attribute_group pwm_fan_group = {
	.attrs = pwm_fan_attributes,
};

static int add_sysfs_entry(struct device *dev)
{
	return sysfs_create_group(&dev->kobj, &pwm_fan_group);
}

static void remove_sysfs_entry(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &pwm_fan_group);
}

static int __devinit pwm_fan_probe(struct platform_device *pdev)
{
	int i;
	struct pwm_fan_platform_data *data;
	struct fan_dev_data *fan_data;
	int *rpm_data;
	int err = 0;

	data = dev_get_platdata(&pdev->dev);
	if (!data) {
		dev_err(&pdev->dev, "platform data is null\n");
		return -EINVAL;
	}

	fan_data = devm_kzalloc(&pdev->dev,
				sizeof(struct fan_dev_data), GFP_KERNEL);
	if (!fan_data)
		return -ENOMEM;

	rpm_data = devm_kzalloc(&pdev->dev,
			4 * sizeof(int) * data->active_steps, GFP_KERNEL);
	if (!rpm_data)
		return -ENOMEM;

	fan_data->fan_rpm = rpm_data;
	fan_data->fan_pwm = rpm_data + data->active_steps;
	fan_data->fan_rru = fan_data->fan_pwm + data->active_steps;
	fan_data->fan_rrd = fan_data->fan_rru + data->active_steps;

	mutex_init(&fan_data->fan_state_lock);

	fan_data->workqueue = alloc_workqueue(dev_name(&pdev->dev),
				WQ_HIGHPRI | WQ_UNBOUND | WQ_RESCUER, 1);
	if (!fan_data->workqueue)
		return -ENOMEM;

	INIT_DELAYED_WORK(&(fan_data->fan_ramp_work), fan_ramping_work_func);

	fan_data->precision_multiplier = data->precision_multiplier;
	fan_data->fan_cap_pwm = data->pwm_cap * data->precision_multiplier;
	fan_data->step_time = data->step_time;
	fan_data->active_steps = data->active_steps;
	fan_data->pwm_period = data->pwm_period;
	fan_data->dev = &pdev->dev;

	for (i = 0; i < fan_data->active_steps; i++) {
		fan_data->fan_rpm[i] = data->active_rpm[i];
		fan_data->fan_pwm[i] = data->active_pwm[i];
		fan_data->fan_rru[i] = data->active_rru[i];
		fan_data->fan_rrd[i] = data->active_rrd[i];
		dev_info(&pdev->dev, "rpm=%d, pwm=%d, rru=%d, rrd=%d\n",
						fan_data->fan_rpm[i],
						fan_data->fan_pwm[i],
						fan_data->fan_rru[i],
						fan_data->fan_rrd[i]);
	}

	fan_data->cdev =
		thermal_cooling_device_register((char *)dev_name(&pdev->dev),
					fan_data, &pwm_fan_cooling_ops);

	if (IS_ERR_OR_NULL(fan_data->cdev)) {
		dev_err(&pdev->dev, "Failed to register cooling device\n");
		return -EINVAL;
	}

	fan_data->pwm_dev = pwm_request(data->pwm_id, dev_name(&pdev->dev));
	if (IS_ERR_OR_NULL(fan_data->pwm_dev)) {
		dev_err(&pdev->dev, "unable to request PWM for fan\n");
		err = -ENODEV;
		goto pwm_req_fail;
	} else {
		dev_info(&pdev->dev, "got pwm for fan\n");
	}

	platform_set_drvdata(pdev, fan_data);

	/*turn temp control on*/
	fan_data->fan_temp_control_flag = 1;
	set_pwm_duty_cycle(0, fan_data);

	if (add_sysfs_entry(&pdev->dev) < 0) {
		dev_err(&pdev->dev, "FAN:Can't create syfs node");
		err = -ENOMEM;
		goto sysfs_fail;
	}

	if (pwm_fan_debug_init(fan_data) < 0) {
		dev_err(&pdev->dev, "FAN:Can't create debug fs nodes");
		/*Just continue without debug fs*/
	}
	return err;

sysfs_fail:
	pwm_free(fan_data->pwm_dev);
pwm_req_fail:
	thermal_cooling_device_unregister(fan_data->cdev);
	return err;
}

static int __devexit pwm_fan_remove(struct platform_device *pdev)
{
	struct fan_dev_data *fan_data = platform_get_drvdata(pdev);

	if (!fan_data)
		return -EINVAL;
	pwm_config(fan_data->pwm_dev, 0, fan_data->pwm_period);
	pwm_disable(fan_data->pwm_dev);
	pwm_free(fan_data->pwm_dev);
	thermal_cooling_device_unregister(fan_data->cdev);
	remove_sysfs_entry(&pdev->dev);
	return 0;
}

#if CONFIG_PM
static int pwm_fan_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct fan_dev_data *fan_data = platform_get_drvdata(pdev);

	mutex_lock(&fan_data->fan_state_lock);
	cancel_delayed_work(&fan_data->fan_ramp_work);
	/*Turn the fan off*/
	fan_data->fan_cur_pwm = 0;
	set_pwm_duty_cycle(0, fan_data);

	/*Stop thermal control*/
	fan_data->fan_temp_control_flag = 0;
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int pwm_fan_resume(struct platform_device *pdev)
{
	struct fan_dev_data *fan_data = platform_get_drvdata(pdev);

	/*Sanity check, want to make sure fan is off when the driver resumes*/
	mutex_lock(&fan_data->fan_state_lock);
	set_pwm_duty_cycle(0, fan_data);

	/*Start thermal control*/
	fan_data->fan_temp_control_flag = 1;
	if (fan_data->next_target_pwm != fan_data->fan_cur_pwm)
		queue_delayed_work(fan_data->workqueue,
					&fan_data->fan_ramp_work,
					msecs_to_jiffies(fan_data->step_time));
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}
#endif

static struct platform_driver pwm_fan_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "pwm-fan",
	},
	.probe = pwm_fan_probe,
	.remove = __devexit_p(pwm_fan_remove),
#if CONFIG_PM
	.suspend = pwm_fan_suspend,
	.resume = pwm_fan_resume,
#endif
};

module_platform_driver(pwm_fan_driver);

MODULE_DESCRIPTION("pwm fan driver");
MODULE_AUTHOR("Anshul Jain <anshulj@nvidia.com>");
MODULE_LICENSE("GPL v2");
