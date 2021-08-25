#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/device.h>
#include "xiaomi_keyboard.h"
#include <drm/drm_notifier.h>
#include <linux/notifier.h>

static struct xiaomi_keyboard_data *mdata;

static void set_keyboard_status(bool on);

static void xiaomi_keyboard_reset(void)
{
	if (!mdata || !mdata->pdata) {
		MI_KB_ERR("reset failed!Invalid Memory\n");
		return;
	}
	MI_KB_INFO("xiaomi keyboard IC Reset\n");
	gpio_direction_output(mdata->pdata->rst_gpio, 0);
	msleep(2);
	gpio_direction_output(mdata->pdata->rst_gpio, 1);
}

static void xiaomi_keyboard_connected_notify(struct device *dev)
{
	sysfs_notify(&dev->kobj, NULL, "xiaomi_keyboard_conn_status");
	MI_KB_INFO("connected status notify\n");
}

static ssize_t xiaomi_keyboard_conn_status_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0, value = 1;
	MI_KB_INFO("%s enter\n", __func__);
	if (!mdata) {
		MI_KB_ERR("Invalid driver info\n");
		return ret;
	}

	mutex_lock(&mdata->rw_mutex);
	value = mdata->keyboard_conn_status;
	mutex_unlock(&mdata->rw_mutex);

	return scnprintf(buf, PAGE_SIZE, "%d", value);
}

static ssize_t xiaomi_keyboard_conn_status_store (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char *cmd;
	cmd = kzalloc(count + 1, GFP_KERNEL);

	if (!cmd) {
		MI_KB_ERR("Allocate Meomory Failed\n");
		goto out;
	}
	memcpy(cmd, buf, count);

	if (!mdata)
		goto out;

	if (!strncmp(cmd, "reset", 5)) {
		xiaomi_keyboard_reset();
	} else if (!strncmp(cmd, "enable_keyboard", 15)) {
		MI_KB_INFO("enable keyboard\n");
		set_keyboard_status(1);
	} else if (!strncmp(cmd, "disable_keyboard", 16)) {
		MI_KB_INFO("disable keyboard\n");
		set_keyboard_status(0);
	}
	else
		MI_KB_ERR("Undefined CMD: %s\n", cmd);
out:
	if (cmd)
		kfree(cmd);
	return count;
}

DEVICE_ATTR(xiaomi_keyboard_conn_status, (S_IRUGO | S_IWUSR | S_IWGRP), xiaomi_keyboard_conn_status_show, xiaomi_keyboard_conn_status_store);

static irqreturn_t xiaomi_keyboard_irq_func(int irq, void *data)
{
	int value = 0;
	MI_KB_INFO("keyboard event: wakeup system\n");
	pm_wakeup_event(&mdata->pdev->dev, 500);
	value = gpio_get_value_cansleep(mdata->pdata->in_irq_gpio);

	mutex_lock(&mdata->rw_mutex);
	mdata->keyboard_conn_status = !mdata->keyboard_conn_status;
	mutex_unlock(&mdata->rw_mutex);

	xiaomi_keyboard_connected_notify(&mdata->pdev->dev);
	MI_KB_INFO("keyboard connected status: %d", mdata->keyboard_conn_status);
	return IRQ_HANDLED;
}

static int xiaomi_keyboard_gpio_config(struct xiaomi_keyboard_platdata *pdata)
{
	int ret = 0;
	if (gpio_is_valid(pdata->rst_gpio)) {
		ret = gpio_request_one(pdata->rst_gpio, GPIOF_OUT_INIT_LOW, "kb_rst");
		if (ret) {
			MI_KB_ERR("Failed to request xiaomi keyboard rst gpio\n");
			goto err_request_rst_gpio;
		}
	}

	if (gpio_is_valid(pdata->in_irq_gpio)) {
		ret = gpio_request_one(pdata->in_irq_gpio, GPIOF_IN, "kb_in_irq");
		if (ret) {
			MI_KB_ERR("Failed to request xiaomi keyboard in-irq gpio\n");
			goto err_request_in_irq_gpio;
		}
	}

	return ret;
err_request_in_irq_gpio:
	gpio_free(pdata->rst_gpio);
err_request_rst_gpio:
	return ret;
}

static void xiaomi_keyboard_gpio_deconfig(struct xiaomi_keyboard_platdata *pdata)
{
	if (gpio_is_valid(pdata->rst_gpio))
		gpio_free(pdata->rst_gpio);

	if (gpio_is_valid(pdata->in_irq_gpio))
		gpio_free(pdata->in_irq_gpio);
}

static int xiaomi_keyboard_setup_gpio(struct xiaomi_keyboard_platdata *pdata)
{
	int ret = 0;
	if (!pdata) {
		MI_KB_ERR("xiaomi keyboard platdata is NULL\n");
		return -EINVAL;
	}
	if (gpio_is_valid(pdata->rst_gpio))
		gpio_direction_output(pdata->rst_gpio, 1);

	mdata->irq = gpio_to_irq(pdata->in_irq_gpio);
	if (mdata->irq) {
		ret = request_threaded_irq(mdata->irq, NULL, xiaomi_keyboard_irq_func,
			IRQF_TRIGGER_RISING|IRQF_ONESHOT, "MiKB-IRQ", mdata);
		if (ret != 0) {
			MI_KB_ERR("request threaded irq failed\n");
			return ret;
		}
	}

	return ret;
}

static int xiaomi_keyboard_resetup_gpio(struct xiaomi_keyboard_platdata *pdata)
{
	int ret = 0;

	if (!mdata || !pdata) {
		MI_KB_ERR("mdata or pdata not ready, return!");
		return -EINVAL;
	}

	if (gpio_is_valid(pdata->rst_gpio))
		gpio_direction_output(pdata->rst_gpio, 0);

	free_irq(mdata->irq, mdata);

	return ret;
}

#ifdef CONFIG_OF
static int xiaomi_keyboard_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct xiaomi_keyboard_platdata *pdata;
	int ret = 0;

	pdata = mdata->pdata;

	pdata->rst_gpio = of_get_named_gpio_flags(np, "xiaomi-keyboard,rst-gpio", 0, &pdata->rst_flags);
	MI_KB_INFO("xiaomi-kb,reset-gpio=%d\n", pdata->rst_gpio);

	pdata->in_irq_gpio = of_get_named_gpio_flags(np, "xiaomi-keyboard,in-irq-gpio", 0, &pdata->in_irq_flags);
	MI_KB_INFO("xiaomi-kb,in-irq-gpio=%d\n", pdata->in_irq_gpio);

	pdata->vdd_gpio = of_get_named_gpio(np, "xiaomi-keyboard,vdd-gpio", 0);
	MI_KB_INFO("xiaomi-kb,vdd-gpio=%d\n", pdata->vdd_gpio);

	return ret;
}
#else
static int xiaomi_keyboard_parse_dt(struct device *dev)
{
	MI_KB_ERR("Xiaomi Keyboard dev is not defined\n");
	return -EINVAL;
}
#endif

static int xiaomi_keyboard_pinctrl_init(struct device *dev)
{
	int ret = 0;

	mdata->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(mdata->pinctrl)) {
		MI_KB_ERR("Failed to get pinctrl, please check dts\n");
		ret = PTR_ERR(mdata->pinctrl);
		goto err_pinctrl_get;
	}
	mdata->pins_active = pinctrl_lookup_state(mdata->pinctrl, "pm_kb_active");
	if (IS_ERR_OR_NULL(mdata->pins_active)) {
		MI_KB_ERR("Pin state [active] not found\n");
		ret = PTR_ERR(mdata->pins_active);
		goto err_pinctrl_lookup;
	}

	mdata->pins_suspend = pinctrl_lookup_state(mdata->pinctrl, "pm_kb_suspend");
	if (IS_ERR_OR_NULL(mdata->pins_suspend)) {
		MI_KB_ERR("Pin state [suspend] not found\n");
		ret = PTR_ERR(mdata->pins_suspend);
		goto err_pinctrl_lookup;
	}

	return 0;
err_pinctrl_lookup:
	if (mdata->pinctrl) {
		devm_pinctrl_put(mdata->pinctrl);
	}
err_pinctrl_get:
	return ret;
}

static int xiaomi_keyboard_power_on(void)
{
	int ret = 0;
	struct xiaomi_keyboard_platdata *pdata;
	pdata = mdata->pdata;
	MI_KB_INFO("Power On\n");
	if (gpio_is_valid(pdata->vdd_gpio)) {
		ret = gpio_request_one(pdata->vdd_gpio, GPIOF_OUT_INIT_HIGH, "kb_vdd_gpio");
		if (ret) {
			MI_KB_ERR("Failed to request xiaomi-keyboard-out-irq gpio\n");
			goto err_request_vdd_gpio;
		}
	}

	gpio_direction_output(pdata->vdd_gpio, 1);
err_request_vdd_gpio:
	return ret;
}

static void xiaomi_keyboard_power_off(void)
{
	struct xiaomi_keyboard_platdata *pdata;
	pdata = mdata->pdata;
	MI_KB_INFO("Power Off\n");
	if (gpio_is_valid(pdata->vdd_gpio)) {
		gpio_direction_output(pdata->vdd_gpio, 0);
		gpio_free(pdata->vdd_gpio);
	}
	return;
}

static int xiaomi_keyboard_suspend(struct device *dev)
{
	int ret = 0;
	MI_KB_INFO("enter\n");
	if (mdata->pinctrl && mdata->pins_suspend) {
		ret = (mdata->keyboard_is_enable && mdata->is_usb_exist)
			? 0 : pinctrl_select_state(mdata->pinctrl, mdata->pins_suspend);
		if (ret < 0) {
			MI_KB_ERR("Set suspend pin state error:%d\n", ret);
		}
	}
	MI_KB_INFO("exit\n");
	return ret;
}

static int xiaomi_keyboard_resume(struct device *dev)
{
	int ret = 0;
	MI_KB_INFO("enter\n");
	if (!mdata->keyboard_is_enable) {
		MI_KB_INFO("keyboard_is_enable is false, stop resume.\n");
		MI_KB_INFO("exit\n");
		return -1;
	}
	if (mdata->pinctrl && mdata->pins_active) {
		ret = pinctrl_select_state(mdata->pinctrl, mdata->pins_active);
		if (ret < 0) {
			MI_KB_ERR("Set active pin state error:%d\n", ret);
		}
	}
	MI_KB_INFO("exit\n");
	return ret;
}

static int xiaomi_keyboard_pm_suspend(struct device *dev)
{
	int ret = 0;
	MI_KB_INFO("enter\n");
	enable_irq_wake(mdata->irq);
	mdata->dev_pm_suspend = true;
	return ret;
}

static int xiaomi_keyboard_pm_resume(struct device *dev)
{
	int ret = 0;
	MI_KB_INFO("enter\n");
	disable_irq_wake(mdata->irq);
	mdata->dev_pm_suspend = false;
	return ret;
}

static const struct dev_pm_ops xiaomi_keyboard_pm_ops = {
	.suspend = xiaomi_keyboard_pm_suspend,
	.resume = xiaomi_keyboard_pm_resume,
};

static int keyboard_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct drm_notify_data *evdata = data;
	int *blank;
	struct xiaomi_keyboard_data *mdata =
		container_of(self, struct xiaomi_keyboard_data, drm_notif);

	if (!evdata)
		return 0;

	if (evdata->data && mdata) {
		blank = evdata->data;
		flush_workqueue(mdata->event_wq);
		if (event == DRM_EARLY_EVENT_BLANK) {
			if (*blank == DRM_BLANK_POWERDOWN) {
				MI_KB_ERR("keyboard suspend");
				mdata->is_in_suspend = true;
				queue_work(mdata->event_wq, &mdata->suspend_work);
			}
		} else if (event == DRM_EVENT_BLANK) {
			if (*blank == DRM_BLANK_UNBLANK) {
				MI_KB_ERR("keyboard resume");
				mdata->is_in_suspend = false;
				flush_workqueue(mdata->event_wq);
				queue_work(mdata->event_wq, &mdata->resume_work);
			}
		}
	}

	return 0;
}

static void keyboard_resume_work(struct work_struct *work)
{
	struct xiaomi_keyboard_data *mdata = container_of(work, struct xiaomi_keyboard_data, resume_work);
	xiaomi_keyboard_resume(&mdata->pdev->dev);
}

static void keyboard_suspend_work(struct work_struct *work)
{
	struct xiaomi_keyboard_data *mdata = container_of(work, struct xiaomi_keyboard_data, resume_work);
	xiaomi_keyboard_suspend(&mdata->pdev->dev);
}

static int kb_power_supply_event(struct notifier_block *nb,
				  unsigned long event, void *ptr)
{
	struct xiaomi_keyboard_data *mdata =
		container_of(nb, struct xiaomi_keyboard_data, power_supply_notifier);

	if (mdata != NULL)
		queue_work(mdata->event_wq, &mdata->power_supply_work);

	return 0;
}

static void kb_power_supply_work(struct work_struct *work)
{
	struct xiaomi_keyboard_data *mdata = container_of(work, struct xiaomi_keyboard_data, power_supply_work);
	int is_usb_exist = 0;

	mutex_lock(&mdata->power_supply_lock);
	is_usb_exist = !!power_supply_is_system_supplied();
	if (is_usb_exist != mdata->is_usb_exist) {
		mdata->is_usb_exist = is_usb_exist;
		MI_KB_INFO("power supply is: %d", mdata->is_usb_exist);
	}
	mutex_unlock(&mdata->power_supply_lock);
}

static void set_keyboard_status(bool on) {
	int ret = 0;

	if (!mdata || !(mdata->pdata)) {
		MI_KB_ERR("mdata or pdata not ready, return!");
		return;
	}

	if (on && !(mdata->keyboard_is_enable)) {
		ret = xiaomi_keyboard_power_on();
		if (ret) {
			MI_KB_ERR("Init 3.3V power failed\n");
			return;
		}
		msleep(1);
		ret = xiaomi_keyboard_setup_gpio(mdata->pdata);
		if (ret) {
			MI_KB_ERR("setup gpio failed\n");
			return;
		}
		msleep(2);

		if (!mdata->is_in_suspend) {
			ret = pinctrl_select_state(mdata->pinctrl, mdata->pins_active);
			if (ret < 0) {
				MI_KB_ERR("Set active pin state error:%d\n", ret);
			}
		}
		mdata->keyboard_is_enable = true;

	} else if (!on && mdata->keyboard_is_enable) {
		if (!mdata->is_in_suspend) {
			ret = pinctrl_select_state(mdata->pinctrl, mdata->pins_suspend);
			if (ret < 0) {
				MI_KB_ERR("Set suspend pin state error:%d\n", ret);
			}
		}

		ret = xiaomi_keyboard_resetup_gpio(mdata->pdata);
		if (ret < 0) {
			MI_KB_ERR("resetup gpio failed\n");
			return;
		}
		xiaomi_keyboard_power_off();
		mdata->keyboard_is_enable = false;
	} else {
		MI_KB_INFO("keyboard status do not need change!");
	}
}

/*******************************************************
Description:
	xiami pad keyboard driver probe function.

return:
	Executive outcomes. 0---succeed. negative---failed
*******************************************************/
static int xiaomi_keyboard_probe(struct platform_device *pdev)
{
	struct xiaomi_keyboard_platdata *pdata;
	int ret = 0;
	MI_KB_INFO("enter\n");
	mdata = kzalloc(sizeof(struct xiaomi_keyboard_data), GFP_KERNEL);
	if (!mdata) {
		MI_KB_ERR("Alloc Memory for xiaomi_keyboard_data failed\n");
		return -ENOMEM;
	}

	pdata = devm_kzalloc(&pdev->dev, sizeof(struct xiaomi_keyboard_platdata), GFP_KERNEL);
	if (!pdata) {
		MI_KB_ERR("Alloc Memory for xiaomi_keyboard_platdata failed\n");
		return -ENOMEM;
	}

	mdata->pdev = pdev;
	mdata->pdata = pdata;
	mutex_init(&mdata->rw_mutex);
	mutex_init(&mdata->power_supply_lock);
	mdata->is_usb_exist = 0;

	ret = xiaomi_keyboard_parse_dt(&pdev->dev);
	if (ret) {
		MI_KB_ERR("parse device tree failed\n");
		goto out;
	}

	ret = xiaomi_keyboard_pinctrl_init(&pdev->dev);
	if (ret) {
		MI_KB_ERR("Pinctrl init failed\n");
		goto out;
	}

	pdata = mdata->pdata;
	ret = xiaomi_keyboard_gpio_config(pdata);
	if (ret) {
		MI_KB_ERR("set gpio config failed\n");
		goto out;
	}

	mdata->dev_pm_suspend = false;
	mdata->keyboard_is_enable = false;
	mdata->is_in_suspend = false;

	ret = sysfs_create_file(&mdata->pdev->dev.kobj, &dev_attr_xiaomi_keyboard_conn_status.attr);
	if (ret < 0) {
		MI_KB_ERR("Create sysfs attribute xiaomi_keyboard_conn_status Failed\n");
		goto err_pinctrl_select;
	}

	mdata->event_wq = alloc_workqueue("kb-event-queue",
		WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!mdata->event_wq) {
		MI_KB_ERR("Can not create work thread for suspend/resume!!");
		ret = -ENOMEM;
		goto err_alloc_work_thread_failed;
	}
	INIT_WORK(&mdata->resume_work, keyboard_resume_work);
	INIT_WORK(&mdata->suspend_work, keyboard_suspend_work);
	INIT_WORK(&mdata->power_supply_work, kb_power_supply_work);

	mdata->drm_notif.notifier_call = keyboard_drm_notifier_callback;
	ret = drm_register_client(&mdata->drm_notif);
	if(ret) {
		MI_KB_ERR("register drm_notifier failed. ret=%d\n", ret);
		goto err_register_drm_notif_failed;
	}

	mdata->power_supply_notifier.notifier_call = kb_power_supply_event;
	ret = power_supply_reg_notifier(&mdata->power_supply_notifier);
	if (ret) {
		MI_KB_ERR("register power_supply_notifier failed. ret=%d\n", ret);
		goto err_register_power_supply_notif_failed;
	}

	MI_KB_INFO("Success\n");
	return ret;

err_register_power_supply_notif_failed:
err_register_drm_notif_failed:
	if (drm_unregister_client(&mdata->drm_notif))
		MI_KB_ERR("Error occurred while unregistering drm_notifier\n");
	if (mdata->event_wq) {
		destroy_workqueue(mdata->event_wq);
	}
err_alloc_work_thread_failed:
	sysfs_remove_file(&mdata->pdev->dev.kobj, &dev_attr_xiaomi_keyboard_conn_status.attr);
err_pinctrl_select:
	if (mdata->pinctrl) {
		devm_pinctrl_put(mdata->pinctrl);
	}
	xiaomi_keyboard_gpio_deconfig(pdata);
out:
	mutex_destroy(&mdata->rw_mutex);
	mutex_destroy(&mdata->power_supply_lock);
	if (mdata) {
		kfree(mdata);
		mdata = NULL;
	}
	MI_KB_ERR("Failed\n");
	return ret;
}

static int xiaomi_keyboard_remove(struct platform_device *pdev)
{
	MI_KB_INFO("enter\n");
	drm_unregister_client(&mdata->drm_notif);
	destroy_workqueue(mdata->event_wq);
	xiaomi_keyboard_gpio_deconfig(mdata->pdata);
	sysfs_remove_file(&mdata->pdev->dev.kobj, &dev_attr_xiaomi_keyboard_conn_status.attr);
	xiaomi_keyboard_power_off();
	devm_pinctrl_put(mdata->pinctrl);
	xiaomi_keyboard_gpio_deconfig(mdata->pdata);
	mutex_destroy(&mdata->rw_mutex);
	mutex_destroy(&mdata->power_supply_lock);
	if (mdata) {
		kfree(mdata);
		mdata = NULL;
	}
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id xiaomi_keyboard_dt_match[] = {
	{ .compatible = "xiaomi,keyboard" },
	{},
};
MODULE_DEVICE_TABLE(of, xiaomi_keyboard_dt_match);
#endif

static const struct platform_device_id xiaomi_keyboard_driver_ids[] = {
	{
		.name = "xiaomi-keyboard",
		.driver_data = 0,
	},
};
MODULE_DEVICE_TABLE(platform, xiaomi_keyboard_driver_ids);


static struct platform_driver xiaomi_keyboard_driver = {
	.probe        = xiaomi_keyboard_probe,
	.remove       = xiaomi_keyboard_remove,
	.driver       = {
		.name = "xiaomi-keyboard",
		.of_match_table = of_match_ptr(xiaomi_keyboard_dt_match),
		.pm = &xiaomi_keyboard_pm_ops,
	},
	.id_table     = xiaomi_keyboard_driver_ids,
};

module_platform_driver(xiaomi_keyboard_driver);

MODULE_DESCRIPTION("Xiaomi Keyboard Control-driver");
MODULE_AUTHOR("Tonghui Wang<wangtonghui@xiaomi.com>");
