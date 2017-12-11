
/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/input/mt.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <uapi/linux/hbtp_input.h>
#include "../input-compat.h"
#include <linux/ktime.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/completion.h>

#include <linux/msm_drm_notify.h>

#define HBTP_INPUT_NAME			"hbtp_input"
#define DISP_COORDS_SIZE		2

#define HBTP_PINCTRL_VALID_STATE_CNT		(2)
#define HBTP_HOLD_DURATION_US			(10)
#define HBTP_PINCTRL_DDIC_SEQ_NUM		(4)
#define HBTP_WAIT_TIMEOUT_MS			2000

struct hbtp_data {
	struct platform_device *pdev;
	struct input_dev *input_dev;
	s32 count;
	struct mutex mutex;
	struct mutex sensormutex;
	struct hbtp_sensor_data *sensor_data;
	bool touch_status[HBTP_MAX_FINGER];
	struct notifier_block dsi_panel_notif;
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
	bool ddic_rst_enabled;
	struct pinctrl_state *ddic_rst_state_active;
	struct pinctrl_state *ddic_rst_state_suspend;
	u32 ts_pinctrl_seq_delay;
	u32 ddic_pinctrl_seq_delay[HBTP_PINCTRL_DDIC_SEQ_NUM];
	u32 dsi_panel_resume_seq_delay;
	bool lcd_on;
	bool power_suspended;
	bool power_sync_enabled;
	bool power_sig_enabled;
	struct completion power_resume_sig;
	struct completion power_suspend_sig;
	struct regulator *vcc_ana;
	struct regulator *vcc_dig;
	int afe_load_ua;
	int afe_vtg_min_uv;
	int afe_vtg_max_uv;
	int dig_load_ua;
	int dig_vtg_min_uv;
	int dig_vtg_max_uv;
	int disp_maxx;		/* Display Max X */
	int disp_maxy;		/* Display Max Y */
	int def_maxx;		/* Default Max X */
	int def_maxy;		/* Default Max Y */
	int des_maxx;		/* Desired Max X */
	int des_maxy;		/* Desired Max Y */
	bool use_scaling;
	bool override_disp_coords;
	bool manage_afe_power_ana;
	bool manage_power_dig;
	bool regulator_enabled;
	u32 power_on_delay;
	u32 power_off_delay;
	bool manage_pin_ctrl;
	struct kobject *sysfs_kobject;
	s16 ROI[MAX_ROI_SIZE];
	s16 accelBuffer[MAX_ACCEL_SIZE];
	u32 display_status;
};

static struct hbtp_data *hbtp;

static struct kobject *sensor_kobject;

static int hbtp_dsi_panel_suspend(struct hbtp_data *ts);
static int hbtp_dsi_panel_early_resume(struct hbtp_data *ts);

static int dsi_panel_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	int blank;
	struct msm_drm_notifier *evdata = data;
	struct hbtp_data *hbtp_data =
	container_of(self, struct hbtp_data, dsi_panel_notif);

	if (!evdata || (evdata->id != 0))
		return 0;

	if (hbtp_data && (event == MSM_DRM_EARLY_EVENT_BLANK)) {
		blank = *(int *)(evdata->data);
		if (blank == MSM_DRM_BLANK_UNBLANK) {
			pr_debug("%s: receives EARLY_BLANK:UNBLANK\n",
					__func__);
			hbtp_data->lcd_on = true;
			hbtp_dsi_panel_early_resume(hbtp_data);
		} else if (blank == MSM_DRM_BLANK_POWERDOWN) {
			pr_debug("%s: receives EARLY_BLANK:POWERDOWN\n",
				__func__);
			hbtp_data->lcd_on = false;
		} else {
			pr_err("%s: receives wrong data EARLY_BLANK:%d\n",
				__func__, blank);
		}
	}

	if (hbtp_data && event == MSM_DRM_EVENT_BLANK) {
		blank = *(int *)(evdata->data);
		if (blank == MSM_DRM_BLANK_POWERDOWN) {
			pr_debug("%s: receives BLANK:POWERDOWN\n", __func__);
			hbtp_dsi_panel_suspend(hbtp_data);
		} else if (blank == MSM_DRM_BLANK_UNBLANK) {
			pr_debug("%s: receives BLANK:UNBLANK\n", __func__);
		} else {
			pr_err("%s: receives wrong data BLANK:%d\n",
				__func__, blank);
		}
	}
	return 0;
}

static ssize_t hbtp_sensor_roi_show(struct file *dev, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t pos,
			size_t size) {
	mutex_lock(&hbtp->sensormutex);
	memcpy(buf, hbtp->ROI, size);
	mutex_unlock(&hbtp->sensormutex);

	return size;
}

static ssize_t hbtp_sensor_vib_show(struct file *dev, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t pos,
			size_t size) {
	mutex_lock(&hbtp->sensormutex);
	memcpy(buf, hbtp->accelBuffer, size);
	mutex_unlock(&hbtp->sensormutex);

	return size;
}

static struct bin_attribute capdata_attr = {
	.attr = {
		.name = "capdata",
		.mode = 0444,
		},
	.size = 1024,
	.read = hbtp_sensor_roi_show,
	.write = NULL,
};

static struct bin_attribute vibdata_attr = {
	.attr = {
		.name = "vib_data",
		.mode = 0444,
		},
	.size = MAX_ACCEL_SIZE*sizeof(int16_t),
	.read = hbtp_sensor_vib_show,
	.write = NULL,
};

static int hbtp_input_open(struct inode *inode, struct file *file)
{
	mutex_lock(&hbtp->mutex);
	if (hbtp->count) {
		pr_err("%s is busy\n", HBTP_INPUT_NAME);
		mutex_unlock(&hbtp->mutex);
		return -EBUSY;
	}
	hbtp->count++;
	mutex_unlock(&hbtp->mutex);

	return 0;
}

static int hbtp_input_release(struct inode *inode, struct file *file)
{
	mutex_lock(&hbtp->mutex);
	if (!hbtp->count) {
		pr_err("%s wasn't opened\n", HBTP_INPUT_NAME);
		mutex_unlock(&hbtp->mutex);
		return -ENOTTY;
	}
	hbtp->count--;
	if (hbtp->power_sig_enabled)
		hbtp->power_sig_enabled = false;
	mutex_unlock(&hbtp->mutex);

	return 0;
}

static int hbtp_input_create_input_dev(struct hbtp_input_absinfo *absinfo)
{
	struct input_dev *input_dev;
	struct hbtp_input_absinfo *abs;
	int error;
	int i;

	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: input_allocate_device failed\n", __func__);
		return -ENOMEM;
	}

	kfree(input_dev->name);
	input_dev->name = kstrndup(HBTP_INPUT_NAME, sizeof(HBTP_INPUT_NAME),
					GFP_KERNEL);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	for (i = KEY_HOME; i <= KEY_MICMUTE; i++)
		__set_bit(i, input_dev->keybit);

	/* For multi touch */
	input_mt_init_slots(input_dev, HBTP_MAX_FINGER, 0);
	for (i = 0; i <= ABS_MT_LAST - ABS_MT_FIRST; i++) {
		abs = absinfo + i;
		if (abs->active) {
			if (abs->code >= 0 && abs->code < ABS_CNT)
				input_set_abs_params(input_dev, abs->code,
					abs->minimum, abs->maximum, 0, 0);
			else
				pr_err("%s: ABS code out of bound\n", __func__);
		}
	}

	if (hbtp->override_disp_coords) {
		input_set_abs_params(input_dev, ABS_MT_POSITION_X,
					0, hbtp->disp_maxx, 0, 0);
		input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
					0, hbtp->disp_maxy, 0, 0);
	}

	error = input_register_device(input_dev);
	if (error) {
		pr_err("%s: input_register_device failed\n", __func__);
		goto err_input_reg_dev;
	}

	hbtp->input_dev = input_dev;
	return 0;

err_input_reg_dev:
	input_free_device(input_dev);

	return error;
}

static int hbtp_input_report_events(struct hbtp_data *hbtp_data,
				struct hbtp_input_mt *mt_data)
{
	int i;
	struct hbtp_input_touch *tch;

	for (i = 0; i < HBTP_MAX_FINGER; i++) {
		tch = &(mt_data->touches[i]);
		if (tch->active || hbtp_data->touch_status[i]) {
			input_mt_slot(hbtp_data->input_dev, i);
			input_mt_report_slot_state(hbtp_data->input_dev,
					MT_TOOL_FINGER, tch->active);

			if (tch->active) {
				input_report_abs(hbtp_data->input_dev,
						ABS_MT_TOOL_TYPE,
						tch->tool);
				input_report_abs(hbtp_data->input_dev,
						ABS_MT_TOUCH_MAJOR,
						tch->major);
				input_report_abs(hbtp_data->input_dev,
						ABS_MT_TOUCH_MINOR,
						tch->minor);
				input_report_abs(hbtp_data->input_dev,
						ABS_MT_ORIENTATION,
						tch->orientation);
				input_report_abs(hbtp_data->input_dev,
						ABS_MT_PRESSURE,
						tch->pressure);
				/*
				 * Scale up/down the X-coordinate as per
				 * DT property
				 */
				if (hbtp_data->use_scaling &&
						hbtp_data->def_maxx > 0 &&
						hbtp_data->des_maxx > 0)
					tch->x = (tch->x * hbtp_data->des_maxx)
							/ hbtp_data->def_maxx;

				input_report_abs(hbtp_data->input_dev,
						ABS_MT_POSITION_X,
						tch->x);
				/*
				 * Scale up/down the Y-coordinate as per
				 * DT property
				 */
				if (hbtp_data->use_scaling &&
						hbtp_data->def_maxy > 0 &&
						hbtp_data->des_maxy > 0)
					tch->y = (tch->y * hbtp_data->des_maxy)
							/ hbtp_data->def_maxy;

				input_report_abs(hbtp_data->input_dev,
						ABS_MT_POSITION_Y,
						tch->y);
			}
			hbtp_data->touch_status[i] = tch->active;
		}
	}

	input_report_key(hbtp->input_dev, BTN_TOUCH, mt_data->num_touches > 0);
	input_sync(hbtp->input_dev);

	return 0;
}

static int reg_set_load_check(struct regulator *reg, int load_uA)
{
	return (regulator_count_voltages(reg) > 0) ?
		regulator_set_load(reg, load_uA) : 0;
}

static int hbtp_pdev_power_on(struct hbtp_data *hbtp, bool on)
{
	int ret;

	if (!hbtp->vcc_ana)
		pr_err("%s: analog regulator is not available\n", __func__);

	if (!hbtp->vcc_dig)
		pr_err("%s: digital regulator is not available\n", __func__);

	if (!hbtp->vcc_ana && !hbtp->vcc_dig) {
		pr_err("%s: no regulators available\n", __func__);
		return -EINVAL;
	}

	if (!on)
		goto reg_off;

	if (hbtp->regulator_enabled) {
		pr_debug("%s: regulator already enabled\n", __func__);
		return 0;
	}

	if (hbtp->vcc_ana) {
		ret = reg_set_load_check(hbtp->vcc_ana,
			hbtp->afe_load_ua);
		if (ret < 0) {
			pr_err("%s: Regulator vcc_ana set_opt failed rc=%d\n",
				__func__, ret);
			return ret;
		}

		ret = regulator_enable(hbtp->vcc_ana);
		if (ret) {
			pr_err("%s: Regulator vcc_ana enable failed rc=%d\n",
				__func__, ret);
			reg_set_load_check(hbtp->vcc_ana, 0);
			return ret;
		}
	}

	if (hbtp->power_on_delay) {
		pr_debug("%s: power-on-delay = %u\n", __func__,
			hbtp->power_on_delay);
		usleep_range(hbtp->power_on_delay,
			hbtp->power_on_delay + HBTP_HOLD_DURATION_US);
	}

	if (hbtp->vcc_dig) {
		ret = reg_set_load_check(hbtp->vcc_dig,
			hbtp->dig_load_ua);
		if (ret < 0) {
			pr_err("%s: Regulator vcc_dig set_opt failed rc=%d\n",
				__func__, ret);
			return ret;
		}

		ret = regulator_enable(hbtp->vcc_dig);
		if (ret) {
			pr_err("%s: Regulator vcc_dig enable failed rc=%d\n",
				__func__, ret);
			reg_set_load_check(hbtp->vcc_dig, 0);
			return ret;
		}
	}

	hbtp->regulator_enabled = true;

	return 0;

reg_off:
	if (!hbtp->regulator_enabled) {
		pr_debug("%s: regulator not enabled\n", __func__);
		return 0;
	}

	if (hbtp->vcc_dig) {
		reg_set_load_check(hbtp->vcc_dig, 0);
		regulator_disable(hbtp->vcc_dig);
	}

	if (hbtp->power_off_delay) {
		pr_debug("%s: power-off-delay = %u\n", __func__,
			hbtp->power_off_delay);
		usleep_range(hbtp->power_off_delay,
			hbtp->power_off_delay + HBTP_HOLD_DURATION_US);
	}

	if (hbtp->vcc_ana) {
		reg_set_load_check(hbtp->vcc_ana, 0);
		regulator_disable(hbtp->vcc_ana);
	}

	hbtp->regulator_enabled = false;

	return 0;
}

static int hbtp_gpio_select(struct hbtp_data *data, bool on)
{
	struct pinctrl_state *pins_state;
	int ret = 0;

	pins_state = on ? data->gpio_state_active : data->gpio_state_suspend;
	if (!IS_ERR_OR_NULL(pins_state)) {
		ret = pinctrl_select_state(data->ts_pinctrl, pins_state);
		if (ret) {
			dev_err(&data->pdev->dev,
				"can not set %s pins\n",
				on ? "ts_active" : "ts_suspend");
			return ret;
		}

		if (on) {
			if (data->ts_pinctrl_seq_delay) {
				usleep_range(data->ts_pinctrl_seq_delay,
					data->ts_pinctrl_seq_delay +
					HBTP_HOLD_DURATION_US);
				dev_dbg(&data->pdev->dev, "ts_pinctrl_seq_delay = %u\n",
					data->ts_pinctrl_seq_delay);
			}
		}
	} else {
		dev_warn(&data->pdev->dev,
			"not a valid '%s' pinstate\n",
			on ? "ts_active" : "ts_suspend");
		return ret;
	}

	return ret;
}

static int hbtp_ddic_rst_select(struct hbtp_data *data, bool on)
{
	struct pinctrl_state *active, *suspend;
	int ret = 0;

	active = data->ddic_rst_state_active;
	if (IS_ERR_OR_NULL(active)) {
		dev_warn(&data->pdev->dev,
			"not a valid ddic_rst_active pinstate\n");
		return ret;
	}

	suspend = data->ddic_rst_state_suspend;
	if (IS_ERR_OR_NULL(suspend)) {
		dev_warn(&data->pdev->dev,
			"not a valid ddic_rst_suspend pinstate\n");
		return ret;
	}

	if (on) {
		if (data->ddic_pinctrl_seq_delay[0]) {
			usleep_range(data->ddic_pinctrl_seq_delay[0],
				data->ddic_pinctrl_seq_delay[0] +
				HBTP_HOLD_DURATION_US);
			dev_dbg(&data->pdev->dev, "ddic_seq_delay[0] = %u\n",
				data->ddic_pinctrl_seq_delay[0]);
		}

		ret = pinctrl_select_state(data->ts_pinctrl, active);
		if (ret) {
			dev_err(&data->pdev->dev,
				"can not set ddic_rst_active pins\n");
			return ret;
		}
		if (data->ddic_pinctrl_seq_delay[1]) {
			usleep_range(data->ddic_pinctrl_seq_delay[1],
				data->ddic_pinctrl_seq_delay[1] +
				HBTP_HOLD_DURATION_US);
			dev_dbg(&data->pdev->dev, "ddic_seq_delay[1] = %u\n",
				data->ddic_pinctrl_seq_delay[1]);
		}
		ret = pinctrl_select_state(data->ts_pinctrl, suspend);
		if (ret) {
			dev_err(&data->pdev->dev,
				"can not set ddic_rst_suspend pins\n");
			return ret;
		}

		if (data->ddic_pinctrl_seq_delay[2]) {
			usleep_range(data->ddic_pinctrl_seq_delay[2],
				data->ddic_pinctrl_seq_delay[2] +
				HBTP_HOLD_DURATION_US);
			dev_dbg(&data->pdev->dev, "ddic_seq_delay[2] = %u\n",
				data->ddic_pinctrl_seq_delay[2]);
		}

		ret = pinctrl_select_state(data->ts_pinctrl, active);
		if (ret) {
			dev_err(&data->pdev->dev,
				"can not set ddic_rst_active pins\n");
			return ret;
		}

		if (data->ddic_pinctrl_seq_delay[3]) {
			usleep_range(data->ddic_pinctrl_seq_delay[3],
				data->ddic_pinctrl_seq_delay[3] +
				HBTP_HOLD_DURATION_US);
			dev_dbg(&data->pdev->dev, "ddic_seq_delay[3] = %u\n",
				data->ddic_pinctrl_seq_delay[3]);
		}
	} else {
		ret = pinctrl_select_state(data->ts_pinctrl, suspend);
		if (ret) {
			dev_err(&data->pdev->dev,
				"can not set ddic_rst_suspend pins\n");
			return ret;
		}
	}

	return ret;
}

static int hbtp_pinctrl_enable(struct hbtp_data *ts, bool on)
{
	int rc = 0;

	if (!ts->manage_pin_ctrl) {
		pr_info("%s: pinctrl info is not available\n", __func__);
		return 0;
	}

	if (!on)
		goto pinctrl_suspend;

	rc = hbtp_gpio_select(ts, true);
	if (rc < 0)
		return -EINVAL;

	if (ts->ddic_rst_enabled) {
		rc = hbtp_ddic_rst_select(ts, true);
		if (rc < 0)
			goto err_ddic_rst_pinctrl_enable;
	}

	return rc;

pinctrl_suspend:
	if (ts->ddic_rst_enabled)
		hbtp_ddic_rst_select(ts, false);
err_ddic_rst_pinctrl_enable:
	hbtp_gpio_select(ts, false);
	return rc;
}

static long hbtp_input_ioctl_handler(struct file *file, unsigned int cmd,
				 unsigned long arg, void __user *p)
{
	int error = 0;
	struct hbtp_input_mt mt_data;
	struct hbtp_input_absinfo absinfo[ABS_MT_LAST - ABS_MT_FIRST + 1];
	struct hbtp_input_key key_data;
	enum hbtp_afe_power_cmd power_cmd;
	enum hbtp_afe_signal afe_signal;
	enum hbtp_afe_power_ctrl afe_power_ctrl;

	switch (cmd) {
	case HBTP_SET_ABSPARAM:
		if (hbtp && hbtp->input_dev) {
			pr_err("%s: The input device is already created\n",
				__func__);
			return 0;
		}

		if (copy_from_user(absinfo, (void *)arg,
					sizeof(struct hbtp_input_absinfo) *
					(ABS_MT_LAST - ABS_MT_FIRST + 1))) {
			pr_err("%s: Error copying data for ABS param\n",
				__func__);
			return -EFAULT;
		}

		error = hbtp_input_create_input_dev(absinfo);
		if (error)
			pr_err("%s, hbtp_input_create_input_dev failed (%d)\n",
				__func__, error);
		break;

	case HBTP_SET_TOUCHDATA:
		if (!hbtp || !hbtp->input_dev) {
			pr_err("%s: The input device hasn't been created\n",
				__func__);
			return -EFAULT;
		}

		if (copy_from_user(&mt_data, (void *)arg,
					sizeof(struct hbtp_input_mt))) {
			pr_err("%s: Error copying data\n", __func__);
			return -EFAULT;
		}

		hbtp_input_report_events(hbtp, &mt_data);
		error = 0;
		break;

	case HBTP_SET_POWERSTATE:
		if (!hbtp || !hbtp->input_dev) {
			pr_err("%s: The input device hasn't been created\n",
				__func__);
			return -EFAULT;
		}

		if (copy_from_user(&power_cmd, (void *)arg,
					sizeof(enum hbtp_afe_power_cmd))) {
			pr_err("%s: Error copying data\n", __func__);
			return -EFAULT;
		}

		switch (power_cmd) {
		case HBTP_AFE_POWER_ON:
			error = hbtp_pdev_power_on(hbtp, true);
			if (error)
				pr_err("%s: failed to power on\n", __func__);
			break;
		case HBTP_AFE_POWER_OFF:
			error = hbtp_pdev_power_on(hbtp, false);
			if (error)
				pr_err("%s: failed to power off\n", __func__);
			break;
		default:
			pr_err("%s: Unsupported command for power state, %d\n",
				__func__, power_cmd);
			return -EINVAL;
		}
		break;

	case HBTP_SET_KEYDATA:
		if (!hbtp || !hbtp->input_dev) {
			pr_err("%s: The input device hasn't been created\n",
				__func__);
			return -EFAULT;
		}

		if (copy_from_user(&key_data, (void *)arg,
					sizeof(struct hbtp_input_key))) {
			pr_err("%s: Error copying data for key info\n",
				__func__);
			return -EFAULT;
		}

		input_report_key(hbtp->input_dev, key_data.code,
				key_data.value);
		input_sync(hbtp->input_dev);
		break;

	case HBTP_SET_SYNCSIGNAL:
		if (!hbtp || !hbtp->input_dev) {
			pr_err("%s: The input device hasn't been created\n",
				__func__);
			return -EFAULT;
		}

		if (!hbtp->power_sig_enabled) {
			pr_err("%s: power_signal is not enabled", __func__);
			return -EPERM;
		}

		if (copy_from_user(&afe_signal, (void *)arg,
					sizeof(enum hbtp_afe_signal))) {
			pr_err("%s: Error copying data\n", __func__);
			return -EFAULT;
		}

		pr_debug("%s: receives %d signal\n", __func__, afe_signal);

		switch (afe_signal) {
		case HBTP_AFE_SIGNAL_ON_RESUME:
			mutex_lock(&hbtp->mutex);
			if (!hbtp->power_suspended) {
				complete(&hbtp->power_resume_sig);
			} else {
				pr_err("%s: resume signal in wrong state\n",
					__func__);
			}
			mutex_unlock(&hbtp->mutex);
			break;
		case HBTP_AFE_SIGNAL_ON_SUSPEND:
			mutex_lock(&hbtp->mutex);
			if (hbtp->power_suspended) {
				complete(&hbtp->power_suspend_sig);
			} else {
				pr_err("%s: suspend signal in wrong state\n",
					__func__);
			}
			mutex_unlock(&hbtp->mutex);
			break;
		default:
			pr_err("%s: Unsupported command for afe signal, %d\n",
				__func__, afe_signal);
			return -EINVAL;
		}
		break;
	case HBTP_SET_POWER_CTRL:
		if (!hbtp || !hbtp->input_dev) {
			pr_err("%s: The input device hasn't been created\n",
				__func__);
			return -EFAULT;
		}

		if (copy_from_user(&afe_power_ctrl, (void *)arg,
					sizeof(enum hbtp_afe_power_ctrl))) {
			pr_err("%s: Error copying data\n", __func__);
			return -EFAULT;
		}
		switch (afe_power_ctrl) {
		case HBTP_AFE_POWER_ENABLE_SYNC:
			pr_debug("%s: power_sync is enabled\n", __func__);
			if (!hbtp->manage_pin_ctrl || !hbtp->manage_power_dig ||
				!hbtp->manage_afe_power_ana) {
				pr_err("%s: power/pin is not available\n",
					__func__);
				return -EFAULT;
			}
			mutex_lock(&hbtp->mutex);
			error = hbtp_pdev_power_on(hbtp, true);
			if (error) {
				mutex_unlock(&hbtp->mutex);
				pr_err("%s: failed to power on\n", __func__);
				return error;
			}
			error  = hbtp_pinctrl_enable(hbtp, true);
			if (error) {
				mutex_unlock(&hbtp->mutex);
				pr_err("%s: failed to enable pins\n", __func__);
				hbtp_pdev_power_on(hbtp, false);
				return error;
			}
			hbtp->power_sync_enabled = true;
			mutex_unlock(&hbtp->mutex);
			pr_debug("%s: power_sync option is enabled\n",
				__func__);
			break;
		case HBTP_AFE_POWER_ENABLE_SYNC_SIGNAL:
			if (!hbtp->power_sync_enabled) {
				pr_err("%s: power_sync is not enabled\n",
					__func__);
				return -EFAULT;
			}
			mutex_lock(&hbtp->mutex);
			init_completion(&hbtp->power_resume_sig);
			init_completion(&hbtp->power_suspend_sig);
			hbtp->power_sig_enabled = true;
			mutex_unlock(&hbtp->mutex);
			pr_err("%s: sync_signal option is enabled\n", __func__);
			break;
		default:
			pr_err("%s: unsupported power ctrl, %d\n",
				__func__, afe_power_ctrl);
			return -EINVAL;
		}
		break;

	case HBTP_SET_SENSORDATA:
		if (copy_from_user(hbtp->sensor_data, (void __user *)arg,
					sizeof(struct hbtp_sensor_data))) {
			pr_err("%s: Error copying data\n", __func__);
			return -EFAULT;
		}
		mutex_lock(&hbtp->sensormutex);
		memcpy(hbtp->ROI, hbtp->sensor_data->ROI, sizeof(hbtp->ROI));
		memcpy(hbtp->accelBuffer, hbtp->sensor_data->accelBuffer,
			sizeof(hbtp->accelBuffer));
		mutex_unlock(&hbtp->sensormutex);

		error = 0;
		break;

	default:
		pr_err("%s: Unsupported ioctl command %u\n", __func__, cmd);
		error = -EINVAL;
		break;
	}

	return error;
}

static long hbtp_input_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	return hbtp_input_ioctl_handler(file, cmd, arg, (void __user *)arg);
}

#ifdef CONFIG_COMPAT
static long hbtp_input_compat_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	return hbtp_input_ioctl_handler(file, cmd, arg, compat_ptr(arg));
}
#endif

static const struct file_operations hbtp_input_fops = {
	.owner		= THIS_MODULE,
	.open		= hbtp_input_open,
	.release	= hbtp_input_release,
	.unlocked_ioctl	= hbtp_input_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= hbtp_input_compat_ioctl,
#endif
};

static struct miscdevice hbtp_input_misc = {
	.fops		= &hbtp_input_fops,
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= HBTP_INPUT_NAME,
};
MODULE_ALIAS_MISCDEV(MISC_DYNAMIC_MINOR);
MODULE_ALIAS("devname:" HBTP_INPUT_NAME);

#ifdef CONFIG_OF
static int hbtp_parse_dt(struct device *dev)
{
	int rc, size;
	struct device_node *np = dev->of_node;
	struct property *prop;
	u32 temp_val;
	u32 disp_reso[DISP_COORDS_SIZE];

	if (of_find_property(np, "vcc_ana-supply", NULL))
		hbtp->manage_afe_power_ana = true;
	if (of_find_property(np, "vcc_dig-supply", NULL))
		hbtp->manage_power_dig = true;

	if (hbtp->manage_afe_power_ana) {
		rc = of_property_read_u32(np, "qcom,afe-load", &temp_val);
		if (!rc) {
			hbtp->afe_load_ua = (int) temp_val;
		} else {
			dev_err(dev, "Unable to read AFE load\n");
			return rc;
		}

		rc = of_property_read_u32(np, "qcom,afe-vtg-min", &temp_val);
		if (!rc) {
			hbtp->afe_vtg_min_uv = (int) temp_val;
		} else {
			dev_err(dev, "Unable to read AFE min voltage\n");
			return rc;
		}

		rc = of_property_read_u32(np, "qcom,afe-vtg-max", &temp_val);
		if (!rc) {
			hbtp->afe_vtg_max_uv = (int) temp_val;
		} else {
			dev_err(dev, "Unable to read AFE max voltage\n");
			return rc;
		}
	}
	if (hbtp->manage_power_dig) {
		rc = of_property_read_u32(np, "qcom,dig-load", &temp_val);
		if (!rc) {
			hbtp->dig_load_ua = (int) temp_val;
		} else {
			dev_err(dev, "Unable to read digital load\n");
			return rc;
		}

		rc = of_property_read_u32(np, "qcom,dig-vtg-min", &temp_val);
		if (!rc) {
			hbtp->dig_vtg_min_uv = (int) temp_val;
		} else {
			dev_err(dev, "Unable to read digital min voltage\n");
			return rc;
		}

		rc = of_property_read_u32(np, "qcom,dig-vtg-max", &temp_val);
		if (!rc) {
			hbtp->dig_vtg_max_uv = (int) temp_val;
		} else {
			dev_err(dev, "Unable to read digital max voltage\n");
			return rc;
		}
	}

	if (hbtp->manage_power_dig && hbtp->manage_afe_power_ana) {
		rc = of_property_read_u32(np,
				"qcom,afe-power-on-delay-us", &temp_val);
		if (!rc)
			hbtp->power_on_delay = (u32)temp_val;
		else
			dev_info(dev, "Power-On Delay is not specified\n");

		rc = of_property_read_u32(np,
				"qcom,afe-power-off-delay-us", &temp_val);
		if (!rc)
			hbtp->power_off_delay = (u32)temp_val;
		else
			dev_info(dev, "Power-Off Delay is not specified\n");

		dev_dbg(dev, "power-on-delay = %u, power-off-delay = %u\n",
			hbtp->power_on_delay, hbtp->power_off_delay);
	}

	prop = of_find_property(np, "qcom,display-resolution", NULL);
	if (prop != NULL) {
		if (!prop->value)
			return -ENODATA;

		size = prop->length / sizeof(u32);
		if (size != DISP_COORDS_SIZE) {
			dev_err(dev, "invalid qcom,display-resolution DT property\n");
			return -EINVAL;
		}

		rc = of_property_read_u32_array(np, "qcom,display-resolution",
							disp_reso, size);
		if (rc && (rc != -EINVAL)) {
			dev_err(dev, "Unable to read DT property qcom,display-resolution\n");
			return rc;
		}

		hbtp->disp_maxx = disp_reso[0];
		hbtp->disp_maxy = disp_reso[1];

		hbtp->override_disp_coords = true;
	}

	hbtp->use_scaling = of_property_read_bool(np, "qcom,use-scale");
	if (hbtp->use_scaling) {
		rc = of_property_read_u32(np, "qcom,default-max-x", &temp_val);
		if (!rc) {
			hbtp->def_maxx = (int) temp_val;
		} else if (rc && (rc != -EINVAL)) {
			dev_err(dev, "Unable to read default max x\n");
			return rc;
		}

		rc = of_property_read_u32(np, "qcom,desired-max-x", &temp_val);
		if (!rc) {
			hbtp->des_maxx = (int) temp_val;
		} else if (rc && (rc != -EINVAL)) {
			dev_err(dev, "Unable to read desired max x\n");
			return rc;
		}

		/*
		 * Either both DT properties i.e. Default max X and
		 * Desired max X should be defined simultaneously, or none
		 * of them should be defined.
		 */
		if ((hbtp->def_maxx == 0 && hbtp->des_maxx != 0) ||
				(hbtp->def_maxx != 0 && hbtp->des_maxx == 0)) {
			dev_err(dev, "default or desired max-X properties are incorrect\n");
			return -EINVAL;
		}

		rc = of_property_read_u32(np, "qcom,default-max-y", &temp_val);
		if (!rc) {
			hbtp->def_maxy = (int) temp_val;
		} else if (rc && (rc != -EINVAL)) {
			dev_err(dev, "Unable to read default max y\n");
			return rc;
		}

		rc = of_property_read_u32(np, "qcom,desired-max-y", &temp_val);
		if (!rc) {
			hbtp->des_maxy = (int) temp_val;
		} else if (rc && (rc != -EINVAL)) {
			dev_err(dev, "Unable to read desired max y\n");
			return rc;
		}

		/*
		 * Either both DT properties i.e. Default max X and
		 * Desired max X should be defined simultaneously, or none
		 * of them should be defined.
		 */
		if ((hbtp->def_maxy == 0 && hbtp->des_maxy != 0) ||
				(hbtp->def_maxy != 0 && hbtp->des_maxy == 0)) {
			dev_err(dev, "default or desired max-Y properties are incorrect\n");
			return -EINVAL;
		}

	}

	return 0;
}
#else
static int hbtp_parse_dt(struct device *dev)
{
	return -ENODEV;
}
#endif

static int hbtp_pinctrl_init(struct hbtp_data *data)
{
	const char *statename;
	int rc;
	int state_cnt, i;
	struct device_node *np = data->pdev->dev.of_node;
	bool pinctrl_state_act_found = false;
	bool pinctrl_state_sus_found = false;
	bool pinctrl_ddic_act_found = false;
	bool pinctrl_ddic_sus_found = false;
	int count = 0;

	data->ts_pinctrl = devm_pinctrl_get(&(data->pdev->dev));
	if (IS_ERR_OR_NULL(data->ts_pinctrl)) {
		dev_err(&data->pdev->dev,
			"Target does not use pinctrl\n");
		rc = PTR_ERR(data->ts_pinctrl);
		data->ts_pinctrl = NULL;
		return rc;
	}

	state_cnt = of_property_count_strings(np, "pinctrl-names");
	if (state_cnt < HBTP_PINCTRL_VALID_STATE_CNT) {
		/*
		 *if pinctrl names are not available then,
		 *power_sync can't be enabled
		 */
		dev_info(&data->pdev->dev,
				"pinctrl names are not available\n");
		rc = -EINVAL;
		goto error;
	}

	for (i = 0; i < state_cnt; i++) {
		rc = of_property_read_string_index(np,
					"pinctrl-names", i, &statename);
		if (rc) {
			dev_err(&data->pdev->dev,
				"failed to read pinctrl states by index\n");
			goto error;
		}

		if (!strcmp(statename, "pmx_ts_active")) {
			data->gpio_state_active
				= pinctrl_lookup_state(data->ts_pinctrl,
								statename);
			if (IS_ERR_OR_NULL(data->gpio_state_active)) {
				dev_err(&data->pdev->dev,
					"Can not get ts default state\n");
				rc = PTR_ERR(data->gpio_state_active);
				goto error;
			}
			pinctrl_state_act_found = true;
		} else if (!strcmp(statename, "pmx_ts_suspend")) {
			data->gpio_state_suspend
				= pinctrl_lookup_state(data->ts_pinctrl,
								statename);
			if (IS_ERR_OR_NULL(data->gpio_state_suspend)) {
				dev_err(&data->pdev->dev,
					"Can not get ts sleep state\n");
				rc = PTR_ERR(data->gpio_state_suspend);
				goto error;
			}
			pinctrl_state_sus_found = true;
		} else if (!strcmp(statename, "ddic_rst_active")) {
			data->ddic_rst_state_active
				= pinctrl_lookup_state(data->ts_pinctrl,
								statename);
			if (IS_ERR(data->ddic_rst_state_active)) {
				dev_err(&data->pdev->dev,
					"Can not get DDIC rst act state\n");
				rc = PTR_ERR(data->ddic_rst_state_active);
				goto error;
			}
			pinctrl_ddic_act_found = true;
		} else if (!strcmp(statename, "ddic_rst_suspend")) {
			data->ddic_rst_state_suspend
				= pinctrl_lookup_state(data->ts_pinctrl,
								statename);
			if (IS_ERR(data->ddic_rst_state_suspend)) {
				dev_err(&data->pdev->dev,
					"Can not get DDIC rst sleep state\n");
				rc = PTR_ERR(data->ddic_rst_state_suspend);
				goto error;
			}
			pinctrl_ddic_sus_found = true;
		} else {
			dev_err(&data->pdev->dev, "invalid pinctrl state\n");
			rc = -EINVAL;
			goto error;
		}
	}

	if (!pinctrl_state_act_found || !pinctrl_state_sus_found) {
		dev_err(&data->pdev->dev,
			"missing required pinctrl states\n");
		rc = -EINVAL;
		goto error;
	}

	if (of_property_read_u32(np, "qcom,pmx-ts-on-seq-delay-us",
			&data->ts_pinctrl_seq_delay)) {
		dev_warn(&data->pdev->dev, "Can not find ts seq delay\n");
	}

	if (of_property_read_u32(np, "qcom,fb-resume-delay-us",
			&data->dsi_panel_resume_seq_delay)) {
		dev_warn(&data->pdev->dev, "Can not find fb resume seq delay\n");
	}

	if (pinctrl_ddic_act_found && pinctrl_ddic_sus_found) {
		count = of_property_count_u32_elems(np,
					"qcom,ddic-rst-on-seq-delay-us");
		if (count == HBTP_PINCTRL_DDIC_SEQ_NUM) {
			of_property_read_u32_array(np,
					"qcom,ddic-rst-on-seq-delay-us",
					data->ddic_pinctrl_seq_delay, count);
		} else {
			dev_err(&data->pdev->dev, "count(%u) is not same as %u\n",
				(u32)count, HBTP_PINCTRL_DDIC_SEQ_NUM);
		}

		data->ddic_rst_enabled = true;
	} else {
		dev_warn(&data->pdev->dev, "ddic pinctrl act/sus not found\n");
	}

	data->manage_pin_ctrl = true;
	return 0;

error:
	devm_pinctrl_put(data->ts_pinctrl);
	data->ts_pinctrl = NULL;
	return rc;
}

static int hbtp_dsi_panel_suspend(struct hbtp_data *ts)
{
	int rc;
	char *envp[2] = {HBTP_EVENT_TYPE_DISPLAY, NULL};

	mutex_lock(&hbtp->mutex);
	if (ts->pdev && ts->power_sync_enabled) {
		pr_debug("%s: power_sync is enabled\n", __func__);
		if (ts->power_suspended) {
			mutex_unlock(&hbtp->mutex);
			pr_err("%s: power is not resumed\n", __func__);
			return 0;
		}
		rc = hbtp_pinctrl_enable(ts, false);
		if (rc) {
			pr_err("%s: failed to disable GPIO pins\n", __func__);
			goto err_pin_disable;
		}

		rc = hbtp_pdev_power_on(ts, false);
		if (rc) {
			pr_err("%s: failed to disable power\n", __func__);
			goto err_power_disable;
		}
		ts->power_suspended = true;
		if (ts->input_dev) {
			kobject_uevent_env(&ts->input_dev->dev.kobj,
					KOBJ_OFFLINE, envp);

			if (ts->power_sig_enabled) {
				pr_debug("%s: power_sig is enabled, wait for signal\n",
					__func__);
				mutex_unlock(&hbtp->mutex);
				rc = wait_for_completion_interruptible_timeout(
					&hbtp->power_suspend_sig,
					msecs_to_jiffies(HBTP_WAIT_TIMEOUT_MS));
				if (rc <= 0) {
					pr_err("%s: wait for suspend is interrupted\n",
						__func__);
				}
				mutex_lock(&hbtp->mutex);
				pr_debug("%s: Wait is done for suspend\n",
					__func__);
			} else {
				pr_debug("%s: power_sig is NOT enabled\n",
					__func__);
			}
		}
	}
	mutex_unlock(&hbtp->mutex);
	return 0;
err_power_disable:
	hbtp_pinctrl_enable(ts, true);
err_pin_disable:
	mutex_unlock(&hbtp->mutex);
	return rc;
}

static int hbtp_dsi_panel_early_resume(struct hbtp_data *ts)
{
	char *envp[2] = {HBTP_EVENT_TYPE_DISPLAY, NULL};
	int rc;

	mutex_lock(&hbtp->mutex);
	if (ts->pdev && ts->power_sync_enabled) {
		pr_debug("%s: power_sync is enabled\n", __func__);
		if (!ts->power_suspended) {
			pr_err("%s: power is not suspended\n", __func__);
			mutex_unlock(&hbtp->mutex);
			return 0;
		}
		rc = hbtp_pdev_power_on(ts, true);
		if (rc) {
			pr_err("%s: failed to enable panel power\n", __func__);
			goto err_power_on;
		}

		rc = hbtp_pinctrl_enable(ts, true);

		if (rc) {
			pr_err("%s: failed to enable pin\n", __func__);
			goto err_pin_enable;
		}

		ts->power_suspended = false;

		if (ts->input_dev) {

			kobject_uevent_env(&ts->input_dev->dev.kobj,
							KOBJ_ONLINE, envp);

			if (ts->power_sig_enabled) {
				pr_err("%s: power_sig is enabled, wait for signal\n",
					__func__);
				mutex_unlock(&hbtp->mutex);
				rc = wait_for_completion_interruptible_timeout(
					&hbtp->power_resume_sig,
					msecs_to_jiffies(HBTP_WAIT_TIMEOUT_MS));
				if (rc <= 0) {
					pr_err("%s: wait for resume is interrupted\n",
						__func__);
				}
				mutex_lock(&hbtp->mutex);
				pr_debug("%s: wait is done\n", __func__);
			} else {
				pr_debug("%s: power_sig is NOT enabled\n",
					__func__);
			}

			if (ts->dsi_panel_resume_seq_delay) {
				usleep_range(ts->dsi_panel_resume_seq_delay,
					ts->dsi_panel_resume_seq_delay +
					HBTP_HOLD_DURATION_US);
				pr_err("%s: dsi_panel_resume_seq_delay = %u\n",
					__func__,
					ts->dsi_panel_resume_seq_delay);
			}
		}
	}
	mutex_unlock(&hbtp->mutex);
	return 0;

err_pin_enable:
	hbtp_pdev_power_on(ts, false);
err_power_on:
	mutex_unlock(&hbtp->mutex);
	return rc;
}

static int hbtp_pdev_probe(struct platform_device *pdev)
{
	int error;
	struct regulator *vcc_ana, *vcc_dig;

	hbtp->pdev = pdev;

	if (pdev->dev.of_node) {
		error = hbtp_parse_dt(&pdev->dev);
		if (error) {
			pr_err("%s: parse dt failed, rc=%d\n", __func__, error);
			return error;
		}
	}

	platform_set_drvdata(pdev, hbtp);

	error = hbtp_pinctrl_init(hbtp);
	if (error) {
		pr_info("%s: pinctrl isn't available, rc=%d\n", __func__,
			error);
	}

	if (hbtp->manage_afe_power_ana) {
		vcc_ana = regulator_get(&pdev->dev, "vcc_ana");
		if (IS_ERR(vcc_ana)) {
			error = PTR_ERR(vcc_ana);
			pr_err("%s: regulator get failed vcc_ana rc=%d\n",
				__func__, error);
			return error;
		}

		if (regulator_count_voltages(vcc_ana) > 0) {
			error = regulator_set_voltage(vcc_ana,
				hbtp->afe_vtg_min_uv, hbtp->afe_vtg_max_uv);
			if (error) {
				pr_err("%s: regulator set vtg failed vcc_ana rc=%d\n",
					__func__, error);
				regulator_put(vcc_ana);
				return error;
			}
		}
		hbtp->vcc_ana = vcc_ana;
	}

	if (hbtp->manage_power_dig) {
		vcc_dig = regulator_get(&pdev->dev, "vcc_dig");
		if (IS_ERR(vcc_dig)) {
			error = PTR_ERR(vcc_dig);
			pr_err("%s: regulator get failed vcc_dig rc=%d\n",
				__func__, error);
			return error;
		}

		if (regulator_count_voltages(vcc_dig) > 0) {
			error = regulator_set_voltage(vcc_dig,
				hbtp->dig_vtg_min_uv, hbtp->dig_vtg_max_uv);
			if (error) {
				pr_err("%s: regulator set vtg failed vcc_dig rc=%d\n",
					__func__, error);
				regulator_put(vcc_dig);
				return error;
			}
		}
		hbtp->vcc_dig = vcc_dig;
	}

	return 0;
}

static int hbtp_pdev_remove(struct platform_device *pdev)
{
	if (hbtp->vcc_ana || hbtp->vcc_dig) {
		hbtp_pdev_power_on(hbtp, false);
		if (hbtp->vcc_ana)
			regulator_put(hbtp->vcc_ana);
		if (hbtp->vcc_dig)
			regulator_put(hbtp->vcc_dig);
	}

	if (hbtp->ts_pinctrl)
		devm_pinctrl_put(hbtp->ts_pinctrl);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id hbtp_match_table[] = {
	{ .compatible = "qcom,hbtp-input",},
	{ },
};
#else
#define hbtp_match_table NULL
#endif

static struct platform_driver hbtp_pdev_driver = {
	.probe		= hbtp_pdev_probe,
	.remove		= hbtp_pdev_remove,
	.driver		= {
		.name		= "hbtp",
		.owner		= THIS_MODULE,
		.of_match_table = hbtp_match_table,
	},
};

static ssize_t hbtp_display_pwr_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	u32 status;
	ssize_t ret;
	char *envp[2] = {HBTP_EVENT_TYPE_DISPLAY, NULL};

	mutex_lock(&hbtp->mutex);
	ret = kstrtou32(buf, 10, &status);
	if (ret) {
		pr_err("hbtp: ret error: %zd\n", ret);
		mutex_unlock(&hbtp->mutex);
		return 0;
	}
	hbtp->display_status = status;
	if (!hbtp->input_dev) {
		pr_err("hbtp: hbtp->input_dev not ready!\n");
		mutex_unlock(&hbtp->mutex);
		return ret;
	}
	if (!hbtp->power_sync_enabled) {
		if (status) {
			pr_debug("hbtp: display power on!\n");
			kobject_uevent_env(&hbtp->input_dev->dev.kobj,
				KOBJ_ONLINE, envp);
		} else {
			pr_debug("hbtp: display power off!\n");
			kobject_uevent_env(&hbtp->input_dev->dev.kobj,
				KOBJ_OFFLINE, envp);
		}
	}
	mutex_unlock(&hbtp->mutex);
	return count;
}

static ssize_t hbtp_display_pwr_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	mutex_lock(&hbtp->mutex);
	ret = snprintf(buf, PAGE_SIZE, "%u\n", hbtp->display_status);
	mutex_unlock(&hbtp->mutex);
	return ret;
}

static struct kobj_attribute hbtp_display_attribute =
		__ATTR(display_pwr, 0660, hbtp_display_pwr_show,
			hbtp_display_pwr_store);


static int __init hbtp_init(void)
{
	int error = 0;

	hbtp = kzalloc(sizeof(struct hbtp_data), GFP_KERNEL);
	if (!hbtp)
		return -ENOMEM;

	hbtp->sensor_data = kzalloc(sizeof(struct hbtp_sensor_data),
			GFP_KERNEL);
	if (!hbtp->sensor_data)
		goto err_sensordata;

	mutex_init(&hbtp->mutex);
	mutex_init(&hbtp->sensormutex);
	hbtp->display_status = 1;

	error = misc_register(&hbtp_input_misc);
	if (error) {
		pr_err("%s: misc_register failed\n", HBTP_INPUT_NAME);
		goto err_misc_reg;
	}

	hbtp->dsi_panel_notif.notifier_call = dsi_panel_notifier_callback;
	error = msm_drm_register_client(&hbtp->dsi_panel_notif);
	if (error) {
		pr_err("%s: Unable to register dsi_panel_notifier: %d\n",
			HBTP_INPUT_NAME, error);
		goto err_dsi_panel_reg;
	}

	sensor_kobject = kobject_create_and_add("hbtpsensor", kernel_kobj);
	if (!sensor_kobject) {
		pr_err("%s: Could not create hbtpsensor kobject\n", __func__);
		goto err_kobject_create;
	}

	error = sysfs_create_bin_file(sensor_kobject, &capdata_attr);
	if (error < 0) {
		pr_err("%s: hbtp capdata sysfs creation failed: %d\n", __func__,
			error);
		goto err_sysfs_create_capdata;
	}
	pr_debug("capdata sysfs creation success\n");

	error = sysfs_create_bin_file(sensor_kobject, &vibdata_attr);
	if (error < 0) {
		pr_err("%s: vibdata sysfs creation failed: %d\n", __func__,
			error);
		goto err_sysfs_create_vibdata;
	}
	pr_debug("vibdata sysfs creation success\n");

	error = platform_driver_register(&hbtp_pdev_driver);
	if (error) {
		pr_err("Failed to register platform driver: %d\n", error);
		goto err_platform_drv_reg;
	}

	hbtp->sysfs_kobject = kobject_create_and_add("hbtp", kernel_kobj);
	if (!hbtp->sysfs_kobject)
		pr_err("%s: Could not create sysfs kobject\n", __func__);
	else {
		error = sysfs_create_file(hbtp->sysfs_kobject,
			&hbtp_display_attribute.attr);
		if (error)
			pr_err("failed to create the display_pwr sysfs\n");
	}

	return 0;

err_platform_drv_reg:
	sysfs_remove_bin_file(sensor_kobject, &vibdata_attr);
err_sysfs_create_vibdata:
	sysfs_remove_bin_file(sensor_kobject, &capdata_attr);
err_sysfs_create_capdata:
	kobject_put(sensor_kobject);
err_kobject_create:
	msm_drm_unregister_client(&hbtp->dsi_panel_notif);
err_dsi_panel_reg:
	misc_deregister(&hbtp_input_misc);
err_misc_reg:
	kfree(hbtp->sensor_data);
err_sensordata:
	kfree(hbtp);

	return error;
}

static void __exit hbtp_exit(void)
{
	sysfs_remove_bin_file(sensor_kobject, &vibdata_attr);
	sysfs_remove_bin_file(sensor_kobject, &capdata_attr);
	kobject_put(sensor_kobject);
	sysfs_remove_file(hbtp->sysfs_kobject, &hbtp_display_attribute.attr);
	kobject_put(hbtp->sysfs_kobject);
	misc_deregister(&hbtp_input_misc);
	if (hbtp->input_dev)
		input_unregister_device(hbtp->input_dev);

	msm_drm_unregister_client(&hbtp->dsi_panel_notif);

	platform_driver_unregister(&hbtp_pdev_driver);

	kfree(hbtp->sensor_data);
	kfree(hbtp);
}

MODULE_DESCRIPTION("Kernel driver to support host based touch processing");
MODULE_LICENSE("GPL v2");

module_init(hbtp_init);
module_exit(hbtp_exit);
