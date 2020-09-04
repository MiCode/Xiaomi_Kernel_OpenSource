/* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(msg) "bgcom_dev:" msg

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>
#include "bgcom.h"
#include "linux/bgcom_interface.h"
#include "bgcom_interface.h"
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#define BGCOM "bg_com_dev"

#define BGDAEMON_LDO09_LPM_VTG 0
#define BGDAEMON_LDO09_NPM_VTG 10000

#define BGDAEMON_LDO03_LPM_VTG 0
#define BGDAEMON_LDO03_NPM_VTG 10000

#define MPPS_DOWN_EVENT_TO_BG_TIMEOUT 3000
#define ADSP_DOWN_EVENT_TO_BG_TIMEOUT 3000

enum {
	SSR_DOMAIN_BG,
	SSR_DOMAIN_MODEM,
	SSR_DOMAIN_ADSP,
	SSR_DOMAIN_MAX,
};

enum ldo_task {
	ENABLE_LDO03,
	ENABLE_LDO09,
	DISABLE_LDO03,
	DISABLE_LDO09
};

struct bgdaemon_regulator {
	struct regulator *regldo03;
	struct regulator *regldo09;
};

struct bgdaemon_priv {
	struct bgdaemon_regulator rgltr;
	enum ldo_task ldo_action;
	void *pil_h;
	bool pending_bg_twm_wear_load;
	struct workqueue_struct *bgdaemon_wq;
	struct work_struct bgdaemon_load_twm_bg_work;
	bool bg_twm_wear_load;
};

struct bg_event {
	enum bg_event_type e_type;
};

struct service_info {
	const char                      name[32];
	int                             domain_id;
	void                            *handle;
	struct notifier_block           *nb;
};

static char *ssr_domains[] = {
	"bg-wear",
	"modem",
	"adsp",
};

static struct bgdaemon_priv *dev;
static unsigned int bgreset_gpio;
static  DEFINE_MUTEX(bg_char_mutex);
static  struct cdev              bg_cdev;
static  struct class             *bg_class;
struct  device                   *dev_ret;
static  dev_t                    bg_dev;
static  int                      device_open;
static  void                     *handle;
static	bool                     twm_exit;
static	bool                     bg_app_running;
static  struct   bgcom_open_config_type   config_type;
static DECLARE_COMPLETION(bg_modem_down_wait);
static DECLARE_COMPLETION(bg_adsp_down_wait);

/**
 * send_uevent(): send events to user space
 * pce : ssr event handle value
 * Return: 0 on success, standard Linux error code on error
 *
 * It adds pce value to event and broadcasts to user space.
 */
static int send_uevent(struct bg_event *pce)
{
	char event_string[32];
	char *envp[2] = { event_string, NULL };

	snprintf(event_string, ARRAY_SIZE(event_string),
			"BG_EVENT=%d", pce->e_type);
	return kobject_uevent_env(&dev_ret->kobj, KOBJ_CHANGE, envp);
}

static void bgcom_load_twm_bg_work(struct work_struct *work)
{
	if (dev->pil_h) {
		pr_err("bg-wear is already loaded\n");
		subsystem_put(dev->pil_h);
		dev->pil_h = NULL;
	} else {
		dev->bg_twm_wear_load = true;
		dev->pil_h = subsystem_get_with_fwname("bg-wear",
							"bg-twm");
		if (!dev->pil_h)
			pr_err("failed to load bg-twm\n");
	}
}

static int bgdaemon_configure_regulators(bool state)
{
	int retval;

	if (state == true) {
		retval = regulator_enable(dev->rgltr.regldo03);
		if (retval)
			pr_err("Failed to enable LDO-03 regulator:%d\n",
					retval);
		retval = regulator_enable(dev->rgltr.regldo09);
		if (retval)
			pr_err("Failed to enable LDO-09 regulator:%d\n",
					retval);
	}
	if (state == false) {
		retval = regulator_disable(dev->rgltr.regldo03);
		if (retval)
			pr_err("Failed to disable LDO-03 regulator:%d\n",
					retval);
		retval = regulator_disable(dev->rgltr.regldo09);
		if (retval)
			pr_err("Failed to disable LDO-09 regulator:%d\n",
					retval);
	}
	return retval;
}

static int bgdaemon_init_regulators(struct device *pdev)
{
	int rc;
	struct regulator *reg03;
	struct regulator *reg09;

	reg03 = regulator_get(pdev, "ssr-reg1");
	if (IS_ERR_OR_NULL(reg03)) {
		rc = PTR_ERR(reg03);
		pr_err("Unable to get regulator for LDO-03\n");
		goto err_ret;
	}
	reg09 = regulator_get(pdev, "ssr-reg2");
	if (IS_ERR_OR_NULL(reg09)) {
		rc = PTR_ERR(reg09);
		pr_err("Unable to get regulator for LDO-09\n");
		goto err_ret;
	}
	dev->rgltr.regldo03 = reg03;
	dev->rgltr.regldo09 = reg09;
	return 0;
err_ret:
	return rc;
}

static int bgdaemon_ldowork(enum ldo_task do_action)
{
	int ret;

	switch (do_action) {
	case ENABLE_LDO03:
		ret = regulator_set_load(dev->rgltr.regldo03,
							BGDAEMON_LDO03_NPM_VTG);
		if (ret < 0) {
			pr_err("Failed to request LDO-03 voltage:%d\n",
					ret);
			goto err_ret;
		}
		break;
	case ENABLE_LDO09:
		ret = regulator_set_load(dev->rgltr.regldo09,
							BGDAEMON_LDO09_NPM_VTG);
		if (ret < 0) {
			pr_err("Failed to request LDO-09 voltage:%d\n",
					ret);
			goto err_ret;
		}
		break;
	case DISABLE_LDO03:
		ret = regulator_set_load(dev->rgltr.regldo03,
							BGDAEMON_LDO03_LPM_VTG);
		if (ret < 0) {
			pr_err("Failed to disable LDO-03:%d\n", ret);
			goto err_ret;
		}
		break;
	case DISABLE_LDO09:
		ret = regulator_set_load(dev->rgltr.regldo09,
							BGDAEMON_LDO09_LPM_VTG);
		if (ret < 0) {
			pr_err("Failed to disable LDO-09:%d\n", ret);
			goto err_ret;
		}
		break;
	default:
		ret = -EINVAL;
	}

err_ret:
	return ret;
}

static int bgcom_char_open(struct inode *inode, struct file *file)
{
	int ret;

	mutex_lock(&bg_char_mutex);
	if (device_open == 1) {
		pr_err("device is already open\n");
		mutex_unlock(&bg_char_mutex);
		return -EBUSY;
	}
	device_open++;
	handle = bgcom_open(&config_type);
	mutex_unlock(&bg_char_mutex);
	if (IS_ERR(handle)) {
		device_open = 0;
		ret = PTR_ERR(handle);
		handle = NULL;
		return ret;
	}
	return 0;
}

static int bgchar_read_cmd(struct bg_ui_data *fui_obj_msg,
		int type)
{
	void              *read_buf;
	int               ret;
	void __user       *result   = (void *)
			(uintptr_t)fui_obj_msg->result;

	read_buf = kmalloc_array(fui_obj_msg->num_of_words, sizeof(uint32_t),
			GFP_KERNEL);
	if (read_buf == NULL)
		return -ENOMEM;
	switch (type) {
	case REG_READ:
		ret = bgcom_reg_read(handle, fui_obj_msg->cmd,
				fui_obj_msg->num_of_words,
				read_buf);
		break;
	case AHB_READ:
		ret = bgcom_ahb_read(handle,
				fui_obj_msg->bg_address,
				fui_obj_msg->num_of_words,
				read_buf);
		break;
	}
	if (!ret && copy_to_user(result, read_buf,
			fui_obj_msg->num_of_words * sizeof(uint32_t))) {
		pr_err("copy to user failed\n");
		ret = -EFAULT;
	}
	kfree(read_buf);
	return ret;
}

static int bgchar_write_cmd(struct bg_ui_data *fui_obj_msg, int type)
{
	void              *write_buf;
	int               ret = -EINVAL;
	void __user       *write     = (void *)
			(uintptr_t)fui_obj_msg->write;

	write_buf = kmalloc_array(fui_obj_msg->num_of_words, sizeof(uint32_t),
			GFP_KERNEL);
	if (write_buf == NULL)
		return -ENOMEM;
	write_buf = memdup_user(write,
			fui_obj_msg->num_of_words * sizeof(uint32_t));
	if (IS_ERR(write_buf)) {
		ret = PTR_ERR(write_buf);
		kfree(write_buf);
		return ret;
	}
	switch (type) {
	case REG_WRITE:
		ret = bgcom_reg_write(handle, fui_obj_msg->cmd,
				fui_obj_msg->num_of_words,
				write_buf);
		break;
	case AHB_WRITE:
		ret = bgcom_ahb_write(handle,
				fui_obj_msg->bg_address,
				fui_obj_msg->num_of_words,
				write_buf);
		break;
	}
	kfree(write_buf);
	return ret;
}

int bg_soft_reset(void)
{
	pr_debug("do BG reset using gpio %d\n", bgreset_gpio);
	if (!gpio_is_valid(bgreset_gpio)) {
		pr_err("gpio %d is not valid\n", bgreset_gpio);
		return -ENXIO;
	}
	if (gpio_direction_output(bgreset_gpio, 1))
		pr_err("gpio %d direction not set\n", bgreset_gpio);

	/* Sleep for 50ms for hardware to detect signal as high */
	msleep(50);

	gpio_set_value(bgreset_gpio, 0);

	/* Sleep for 50ms for hardware to detect signal as high */
	msleep(50);
	gpio_set_value(bgreset_gpio, 1);

	return 0;
}
EXPORT_SYMBOL(bg_soft_reset);

static int modem_down2_bg(void)
{
	complete(&bg_modem_down_wait);
	return 0;
}

static int adsp_down2_bg(void)
{
	complete(&bg_adsp_down_wait);
	return 0;
}

static long bg_com_ioctl(struct file *filp,
		unsigned int ui_bgcom_cmd, unsigned long arg)
{
	int ret;
	struct bg_ui_data ui_obj_msg;

	switch (ui_bgcom_cmd) {
	case REG_READ:
	case AHB_READ:
		if (copy_from_user(&ui_obj_msg, (void __user *) arg,
				sizeof(ui_obj_msg))) {
			pr_err("The copy from user failed\n");
			ret = -EFAULT;
		}
		ret = bgchar_read_cmd(&ui_obj_msg,
				ui_bgcom_cmd);
		if (ret < 0)
			pr_err("bgchar_read_cmd failed\n");
		break;
	case AHB_WRITE:
	case REG_WRITE:
		if (copy_from_user(&ui_obj_msg, (void __user *) arg,
				sizeof(ui_obj_msg))) {
			pr_err("The copy from user failed\n");
			ret = -EFAULT;
		}
		ret = bgchar_write_cmd(&ui_obj_msg, ui_bgcom_cmd);
		if (ret < 0)
			pr_err("bgchar_write_cmd failed\n");
		break;
	case SET_SPI_FREE:
		ret = bgcom_set_spi_state(BGCOM_SPI_FREE);
		break;
	case SET_SPI_BUSY:
		ret = bgcom_set_spi_state(BGCOM_SPI_BUSY);
		break;
	case BG_SOFT_RESET:
		ret = bg_soft_reset();
		break;
	case BG_MODEM_DOWN2_BG_DONE:
		ret = modem_down2_bg();
		break;
	case BG_ADSP_DOWN2_BG_DONE:
		ret = adsp_down2_bg();
	case BG_TWM_EXIT:
		twm_exit = true;
		ret = 0;
		break;
	case BG_APP_RUNNING:
		bg_app_running = true;
		ret = 0;
		break;
	case BG_WEAR_LOAD:
		ret = 0;
		if (dev->pil_h) {
			pr_err("bg-wear is already loaded\n");
			ret = -EFAULT;
			break;
		}
		dev->bg_twm_wear_load = false;
		dev->pil_h = subsystem_get_with_fwname("bg-wear", "bg-wear");
		if (!dev->pil_h) {
			pr_err("failed to load bg-wear\n");
			ret = -EFAULT;
		}
		break;
	case BG_WEAR_TWM_LOAD:
		dev->pending_bg_twm_wear_load = true;
		queue_work(dev->bgdaemon_wq, &dev->bgdaemon_load_twm_bg_work);
		ret = 0;
		break;
	case BG_WEAR_UNLOAD:
		if (dev->pil_h) {
			subsystem_put(dev->pil_h);
			dev->pil_h = NULL;
			bg_soft_reset();
		}
		ret = 0;
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}

static int bgcom_char_close(struct inode *inode, struct file *file)
{
	int ret;

	mutex_lock(&bg_char_mutex);
	ret = bgcom_close(&handle);
	device_open = 0;
	mutex_unlock(&bg_char_mutex);
	return ret;
}

static int bg_daemon_probe(struct platform_device *pdev)
{
	struct device_node *node;
	unsigned int reset_gpio;
	int ret;

	node = pdev->dev.of_node;

	pr_info("%s started", __func__);

	dev = kzalloc(sizeof(struct bgdaemon_priv), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	reset_gpio = of_get_named_gpio(node, "qcom,bg-reset-gpio", 0);
	if (!gpio_is_valid(reset_gpio)) {
		pr_err("gpio %d found is not valid\n", reset_gpio);
		goto err_ret;
	}

	if (gpio_request(reset_gpio, "bg_reset_gpio")) {
		pr_err("gpio %d request failed\n", reset_gpio);
		goto err_ret;
	}

	if (gpio_direction_output(reset_gpio, 1)) {
		pr_err("gpio %d direction not set\n", reset_gpio);
		goto err_ret;
	}

	pr_info("bg-soft-reset gpio successfully requested\n");
	bgreset_gpio = reset_gpio;

	ret = bgdaemon_init_regulators(&pdev->dev);
	if (ret != 0) {
		pr_err("Failed to init regulators:%d\n", ret);
		goto err_device;
	}
	ret = bgdaemon_configure_regulators(true);
	if (ret) {
		pr_err("Failed to confifigure regulators:%d\n", ret);
		bgdaemon_configure_regulators(false);
		goto err_ret;
	}
	pr_info("%s success", __func__);

err_device:
	return -ENODEV;
err_ret:
	return 0;
}

static const struct of_device_id bg_daemon_of_match[] = {
	{ .compatible = "qcom,bg-daemon", },
	{ }
};
MODULE_DEVICE_TABLE(of, bg_daemon_of_match);

static struct platform_driver bg_daemon_driver = {
	.probe  = bg_daemon_probe,
	.driver = {
		.name = "bg-daemon",
		.of_match_table = bg_daemon_of_match,
	},
};

static const struct file_operations fops = {
	.owner          = THIS_MODULE,
	.open           = bgcom_char_open,
	.release        = bgcom_char_close,
	.unlocked_ioctl = bg_com_ioctl,
};

static int __init init_bg_com_dev(void)
{
	int ret;

	ret = alloc_chrdev_region(&bg_dev, 0, 1, BGCOM);
	if (ret  < 0) {
		pr_err("failed with error %d\n", ret);
		return ret;
	}
	cdev_init(&bg_cdev, &fops);
	ret = cdev_add(&bg_cdev, bg_dev, 1);
	if (ret < 0) {
		unregister_chrdev_region(bg_dev, 1);
		pr_err("device registration failed\n");
		return ret;
	}
	bg_class = class_create(THIS_MODULE, BGCOM);
	if (IS_ERR_OR_NULL(bg_class)) {
		cdev_del(&bg_cdev);
		unregister_chrdev_region(bg_dev, 1);
		pr_err("class creation failed\n");
		return PTR_ERR(bg_class);
	}

	dev_ret = device_create(bg_class, NULL, bg_dev, NULL, BGCOM);
	if (IS_ERR_OR_NULL(dev_ret)) {
		class_destroy(bg_class);
		cdev_del(&bg_cdev);
		unregister_chrdev_region(bg_dev, 1);
		pr_err("device create failed\n");
		return PTR_ERR(dev_ret);
	}

	if (platform_driver_register(&bg_daemon_driver))
		pr_err("%s: failed to register bg-daemon register\n", __func__);

	dev->bgdaemon_wq =
		create_singlethread_workqueue("bgdaemon-work-queue");
	if (!dev->bgdaemon_wq) {
		pr_err("Failed to init BG-DAEMON work-queue\n");
		goto free_wq;
	}
	INIT_WORK(&dev->bgdaemon_load_twm_bg_work, bgcom_load_twm_bg_work);

	return 0;

free_wq:
	destroy_workqueue(dev->bgdaemon_wq);
	return -EFAULT;
}

static void __exit exit_bg_com_dev(void)
{
	device_destroy(bg_class, bg_dev);
	class_destroy(bg_class);
	cdev_del(&bg_cdev);
	unregister_chrdev_region(bg_dev, 1);
	bgdaemon_configure_regulators(false);
	platform_driver_unregister(&bg_daemon_driver);
}

/**
 *ssr_bg_cb(): callback function is called
 *by ssr framework when BG goes down, up and during ramdump
 *collection. It handles BG shutdown and power up events.
 */
static int ssr_bg_cb(struct notifier_block *this,
		unsigned long opcode, void *data)
{
	struct bg_event bge;

	switch (opcode) {
	case SUBSYS_BEFORE_SHUTDOWN:
		bge.e_type = BG_BEFORE_POWER_DOWN;
		bgdaemon_ldowork(ENABLE_LDO03);
		bgdaemon_ldowork(ENABLE_LDO09);
		bgcom_bgdown_handler();
		bgcom_set_spi_state(BGCOM_SPI_BUSY);
		send_uevent(&bge);
		break;
	case SUBSYS_AFTER_SHUTDOWN:
		if (dev->pending_bg_twm_wear_load) {
			bg_soft_reset();
			/* Load bg-twm */
			dev->pending_bg_twm_wear_load = false;
			queue_work(dev->bgdaemon_wq,
				&dev->bgdaemon_load_twm_bg_work);
		}
		break;
	case SUBSYS_AFTER_POWERUP:
		if (dev->bg_twm_wear_load)
			bge.e_type = TWM_BG_AFTER_POWER_UP;
		else
			bge.e_type = BG_AFTER_POWER_UP;
		bgdaemon_ldowork(DISABLE_LDO03);
		bgdaemon_ldowork(DISABLE_LDO09);
		bgcom_set_spi_state(BGCOM_SPI_FREE);
		send_uevent(&bge);
		break;
	}
	return NOTIFY_DONE;
}

/**
 *ssr_modem_cb(): callback function is called
 *by ssr framework when modem goes down, up and during ramdump
 *collection. It handles modem shutdown and power up events.
 */
static int ssr_modem_cb(struct notifier_block *this,
		unsigned long opcode, void *data)
{
	struct bg_event modeme;
	int ret;

	switch (opcode) {
	case SUBSYS_BEFORE_SHUTDOWN:
		modeme.e_type = MODEM_BEFORE_POWER_DOWN;
		reinit_completion(&bg_modem_down_wait);
		send_uevent(&modeme);
		ret = wait_for_completion_timeout(&bg_modem_down_wait,
			msecs_to_jiffies(MPPS_DOWN_EVENT_TO_BG_TIMEOUT));
		if (!ret)
			pr_err("Time out on modem down event\n");
		break;
	case SUBSYS_AFTER_POWERUP:
		modeme.e_type = MODEM_AFTER_POWER_UP;
		send_uevent(&modeme);
		break;
	}
	return NOTIFY_DONE;
}

static int ssr_adsp_cb(struct notifier_block *this,
		unsigned long opcode, void *data)
{
	struct bg_event adspe;
	int ret;

	switch (opcode) {
	case SUBSYS_BEFORE_SHUTDOWN:
		adspe.e_type = ADSP_BEFORE_POWER_DOWN;
		reinit_completion(&bg_adsp_down_wait);
		send_uevent(&adspe);
		ret = wait_for_completion_timeout(&bg_adsp_down_wait,
			msecs_to_jiffies(ADSP_DOWN_EVENT_TO_BG_TIMEOUT));
		if (!ret)
			pr_err("Time out on adsp down event\n");
		break;
	case SUBSYS_AFTER_POWERUP:
		adspe.e_type = ADSP_AFTER_POWER_UP;
		send_uevent(&adspe);
		break;
	}
	return NOTIFY_DONE;
}
bool is_twm_exit(void)
{
	if (twm_exit) {
		twm_exit = false;
		return true;
	}
	return false;
}
EXPORT_SYMBOL(is_twm_exit);

bool is_bg_running(void)
{
	if (bg_app_running) {
		bg_app_running = false;
		return true;
	}
	return false;
}
EXPORT_SYMBOL(is_bg_running);

static struct notifier_block ssr_modem_nb = {
	.notifier_call = ssr_modem_cb,
	.priority = 0,
};

static struct notifier_block ssr_adsp_nb = {
	.notifier_call = ssr_adsp_cb,
	.priority = 0,
};

static struct notifier_block ssr_bg_nb = {
	.notifier_call = ssr_bg_cb,
	.priority = 0,
};

static struct service_info service_data[3] = {
	{
		.name = "SSR_BG",
		.domain_id = SSR_DOMAIN_BG,
		.nb = &ssr_bg_nb,
		.handle = NULL,
	},
	{
		.name = "SSR_MODEM",
		.domain_id = SSR_DOMAIN_MODEM,
		.nb = &ssr_modem_nb,
		.handle = NULL,
	},
	{
		.name = "SSR_ADSP",
		.domain_id = SSR_DOMAIN_ADSP,
		.nb = &ssr_adsp_nb,
		.handle = NULL,
	},
};

/**
 * ssr_register checks that domain id should be in range and register
 * SSR framework for value at domain id.
 */
static int __init ssr_register(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(service_data); i++) {
		if ((service_data[i].domain_id < 0) ||
				(service_data[i].domain_id >= SSR_DOMAIN_MAX)) {
			pr_err("Invalid service ID = %d\n",
					service_data[i].domain_id);
		} else {
			service_data[i].handle =
					subsys_notif_register_notifier(
					ssr_domains[service_data[i].domain_id],
					service_data[i].nb);
			if (IS_ERR_OR_NULL(service_data[i].handle)) {
				pr_err("subsys register failed for id = %d",
						service_data[i].domain_id);
				service_data[i].handle = NULL;
			}
		}
	}
	return 0;
}

module_init(init_bg_com_dev);
late_initcall(ssr_register);
module_exit(exit_bg_com_dev);
MODULE_LICENSE("GPL v2");
