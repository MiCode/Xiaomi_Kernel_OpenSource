// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#define pr_fmt(msg) "helioscom_dev:" msg

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
#include "helioscom.h"
#include "linux/helioscom_interface.h"
#include "helioscom_interface.h"
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <asm/dma.h>
#include <linux/dma-mapping.h>

#include <linux/remoteproc.h>
#include <linux/remoteproc/qcom_rproc.h>

#define HELIOSCOM "helios_com_dev"

#define HELIOSDAEMON_LDO09_LPM_VTG 0
#define HELIOSDAEMON_LDO09_NPM_VTG 10000

#define HELIOSDAEMON_LDO03_LPM_VTG 0
#define HELIOSDAEMON_LDO03_NPM_VTG 10000

#define MPPS_DOWN_EVENT_TO_HELIOS_TIMEOUT 3000
#define ADSP_DOWN_EVENT_TO_HELIOS_TIMEOUT 3000
#define MAX_APP_NAME_SIZE 100

/*pil_helios_intf.h*/
#define RESULT_SUCCESS 0
#define RESULT_FAILURE -1

#define HELIOSCOM_INTF_N_FILES 3
#define BUF_SIZE 10

static char btss_state[BUF_SIZE] = "offline";
static char dspss_state[BUF_SIZE] = "offline";
static void ssr_register(void);

/* tzapp command list.*/
enum helios_tz_commands {
	HELIOSPIL_RAMDUMP,
	HELIOSPIL_IMAGE_LOAD,
	HELIOSPIL_AUTH_MDT,
	HELIOSPIL_DLOAD_CONT,
	HELIOSPIL_GET_HELIOS_VERSION,
	HELIOSPIL_TWM_DATA,
};

/* tzapp helios request.*/
struct tzapp_helios_req {
	uint8_t tzapp_helios_cmd;
	uint8_t padding[3];
	phys_addr_t address_fw;
	size_t size_fw;
} __attribute__ ((__packed__));

/* tzapp helios response.*/
struct tzapp_helios_rsp {
	uint32_t tzapp_helios_cmd;
	uint32_t helios_info_len;
	int32_t status;
	uint32_t helios_info[100];
} __attribute__ ((__packed__));

enum {
	SSR_DOMAIN_HELIOS,
	SSR_DOMAIN_MODEM,
	SSR_DOMAIN_ADSP,
	SSR_DOMAIN_MAX,
};

enum helioscom_state {
	HELIOSCOM_STATE_UNKNOWN,
	HELIOSCOM_STATE_INIT,
	HELIOSCOM_STATE_GLINK_OPEN,
	HELIOSCOM_STATE_HELIOS_SSR
};

struct heliosdaemon_priv {
	void *pil_h;
	int app_status;
	unsigned long attrs;
	u32 cmd_status;
	struct device *platform_dev;
	bool helioscom_rpmsg;
	bool helios_resp_cmplt;
	void *lhndl;
	wait_queue_head_t link_state_wait;
	char rx_buf[20];
	struct work_struct helioscom_up_work;
	struct work_struct helioscom_down_work;
	struct mutex glink_mutex;
	struct mutex helioscom_state_mutex;
	enum helioscom_state helioscom_current_state;
	struct workqueue_struct *helioscom_wq;
	struct wakeup_source *helioscom_ws;
};


struct helios_event {
	enum helios_event_type e_type;
};

struct service_info {
	const char                      name[32];
	int                             domain_id;
	void                            *handle;
	struct notifier_block           *nb;
};

static char *ssr_domains[] = {
	"helios",
	"modem",
	"adsp",
};

static struct heliosdaemon_priv *dev;
static unsigned int heliosreset_gpio;
static  DEFINE_MUTEX(helios_char_mutex);
static  struct cdev              helios_cdev;
static  struct class             *helios_class;
struct  device                   *dev_ret;
static  dev_t                    helios_dev;
static  int                      device_open;
static  void                     *handle;
static	bool                     helios_app_running;
static  struct   helioscom_open_config_type   config_type;
static DECLARE_COMPLETION(helios_modem_down_wait);
static DECLARE_COMPLETION(helios_adsp_down_wait);
static struct platform_device *helios_pdev;

static ssize_t helios_bt_state_sysfs_read
			(struct class *class, struct class_attribute *attr, char *buf)
{
	pr_debug("In %s\n", __func__);
	return scnprintf(buf, BUF_SIZE, btss_state);
}

static ssize_t helios_dsp_state_sysfs_read
			(struct class *class, struct class_attribute *attr, char *buf)
{
	pr_debug("In %s\n", __func__);
	return	scnprintf(buf, BUF_SIZE, dspss_state);
}

static ssize_t helios_sleep_state_sysfs_write
			(struct class *class, struct class_attribute *attr, const char *buf,
			 size_t count)
{
	long tmp;
	int ret;

	pr_info("In %s\n", __func__);
	ret = kstrtol(buf, 10, &tmp);

	if (ret != 0)
		return ret;

	if (tmp == 1) {
		/* Set helios is sleep state */
		ret = set_helios_sleep_state(true);
	}

	if (tmp == 0) {
		/* Wakeup helios */
		ret = set_helios_sleep_state(false);
	}
	return count;
}

static ssize_t helios_sleep_state_sysfs_read
			(struct class *class, struct class_attribute *attr, char *buf)
{
	int ret = 0;

	pr_info("In %s\n", __func__);
	ret = get_helios_sleep_state();
	return sysfs_emit(buf, "%d\n", ret);
}

struct class_attribute helioscom_attr[] = {
	{
		.attr = {
			.name = "helios_bt_state",
			.mode = 0644
		},
		.show	= helios_bt_state_sysfs_read,
	},
	{
		.attr = {
			.name = "helios_dsp_state",
			.mode = 0644
		},
		.show	= helios_dsp_state_sysfs_read,
	},
	{
		.attr = {
			.name = "helios_sleep_state",
			.mode = 0644
		},
		.store	= helios_sleep_state_sysfs_write,
		.show	= helios_sleep_state_sysfs_read,
	},
};
struct class helioscom_intf_class = {
	.name = "helioscom"
};

/**
 * send_uevent(): send events to user space
 * pce : ssr event handle value
 * Return: 0 on success, standard Linux error code on error
 *
 * It adds pce value to event and broadcasts to user space.
 */
static int send_uevent(struct helios_event *pce)
{
	char event_string[32];
	char *envp[2] = { event_string, NULL };

	snprintf(event_string, ARRAY_SIZE(event_string),
			"HELIOS_EVENT=%d", pce->e_type);
	return kobject_uevent_env(&dev_ret->kobj, KOBJ_CHANGE, envp);
}


static int helioscom_char_open(struct inode *inode, struct file *file)
{
	int ret;

	mutex_lock(&helios_char_mutex);
	if (device_open == 1) {
		pr_err("device is already open\n");
		mutex_unlock(&helios_char_mutex);
		return -EBUSY;
	}
	device_open++;
	handle = helioscom_open(&config_type);
	mutex_unlock(&helios_char_mutex);
	if (IS_ERR(handle)) {
		device_open = 0;
		ret = PTR_ERR(handle);
		handle = NULL;
		return ret;
	}
	return 0;
}

static int helioschar_read_cmd(struct helios_ui_data *fui_obj_msg,
		unsigned int type)
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
		ret = helioscom_reg_read(handle, fui_obj_msg->cmd,
				fui_obj_msg->num_of_words,
				read_buf);
		break;
	case AHB_READ:
		ret = helioscom_ahb_read(handle,
				fui_obj_msg->helios_address,
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

static int helioschar_write_cmd(struct helios_ui_data *fui_obj_msg, unsigned int type)
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
		ret = helioscom_reg_write(handle, fui_obj_msg->cmd,
				fui_obj_msg->num_of_words,
				write_buf);
		break;
	case AHB_WRITE:
		ret = helioscom_ahb_write(handle,
				fui_obj_msg->helios_address,
				fui_obj_msg->num_of_words,
				write_buf);
		break;
	}
	kfree(write_buf);
	return ret;
}

static void helios_load_fw(struct  heliosdaemon_priv *priv)
{
	struct platform_device *pdev = NULL;
	int ret;
	const char *firmware_name = NULL;
	phandle rproc_phandle;

	if (!priv) {
		pr_err(" %s: Private data get failed\n", __func__);
		goto fail;
	}

	pdev = helios_pdev;

	if (!pdev) {
		pr_err("%s: Platform device null\n", __func__);
		goto fail;
	}

	if (!pdev->dev.of_node) {
		pr_err("%s: Device tree information missing\n", __func__);
		goto fail;
	}

	ret = of_property_read_string(pdev->dev.of_node,
		"qcom,firmware-name", &firmware_name);
	if (ret < 0) {
		pr_err("can't get fw name.\n");
		goto fail;
	}


	if (!priv->pil_h) {
		if (of_property_read_u32(pdev->dev.of_node, "qcom,rproc-handle",
					 &rproc_phandle)) {
			pr_err("error reading rproc phandle\n");
			goto fail;
		}

		priv->pil_h = rproc_get_by_phandle(rproc_phandle);
		if (!priv->pil_h) {
			pr_err("rproc not found\n");
			goto fail;
		}
	}

	ret = rproc_boot(priv->pil_h);
	if (ret) {
		pr_err("%s: rproc boot failed, err: %d\n",
			__func__, ret);
		goto fail;
	}

	pr_debug("%s: Helios image is loaded\n", __func__);
	return;

fail:
	pr_err("%s: HELIOS image loading failed\n", __func__);
}

static void helios_loader_unload(struct  heliosdaemon_priv *priv)
{
	if (!priv)
		return;

	if (priv->pil_h) {
		pr_debug("%s: calling subsystem put\n", __func__);
		rproc_shutdown(priv->pil_h);
		priv->pil_h = NULL;
	}
}

int helios_soft_reset(void)
{
	pr_debug("do HELIOS reset using gpio %d\n", heliosreset_gpio);
	if (!gpio_is_valid(heliosreset_gpio)) {
		pr_err("gpio %d is not valid\n", heliosreset_gpio);
		return -ENXIO;
	}
	if (gpio_direction_output(heliosreset_gpio, 1))
		pr_err("gpio %d direction not set\n", heliosreset_gpio);

	/* Sleep for 50ms for hardware to detect signal as high */
	msleep(50);

	gpio_set_value(heliosreset_gpio, 0);

	/* Sleep for 50ms for hardware to detect signal as high */
	msleep(50);
	gpio_set_value(heliosreset_gpio, 1);

	return 0;
}
EXPORT_SYMBOL(helios_soft_reset);



static long helios_com_ioctl(struct file *filp,
		unsigned int ui_helioscom_cmd, unsigned long arg)
{
	int ret;
	struct helios_ui_data ui_obj_msg;

	if (filp == NULL)
		return -EINVAL;

	switch (ui_helioscom_cmd) {
	case REG_READ:
	case AHB_READ:
		if (copy_from_user(&ui_obj_msg, (void __user *) arg,
				sizeof(ui_obj_msg))) {
			pr_err("The copy from user failed\n");
			ret = -EFAULT;
		}
		ret = helioschar_read_cmd(&ui_obj_msg,
				ui_helioscom_cmd);
		if (ret < 0)
			pr_err("helioschar_read_cmd failed\n");
		break;
	case AHB_WRITE:
	case REG_WRITE:
		if (copy_from_user(&ui_obj_msg, (void __user *) arg,
				sizeof(ui_obj_msg))) {
			pr_err("The copy from user failed\n");
			ret = -EFAULT;
		}
		ret = helioschar_write_cmd(&ui_obj_msg, ui_helioscom_cmd);
		if (ret < 0)
			pr_err("helioschar_write_cmd failed\n");
		break;
	case SET_SPI_FREE:
		ret = helioscom_set_spi_state(HELIOSCOM_SPI_FREE);
		break;
	case SET_SPI_BUSY:
		ret = helioscom_set_spi_state(HELIOSCOM_SPI_BUSY);
		break;
	case HELIOS_SOFT_RESET:
		ret = helios_soft_reset();
		break;
	case HELIOS_APP_RUNNING:
		helios_app_running = true;
		ret = 0;
		break;
	case HELIOS_LOAD:
		ret = 0;
		if (dev->pil_h) {
			pr_err("helios is already loaded\n");
			ret = -EFAULT;
			break;
		}
		helios_load_fw(dev);
		if (!dev->pil_h) {
			pr_err("failed to load helios\n");
			ret = -EFAULT;
		}
		break;
	case HELIOS_UNLOAD:
		helios_loader_unload(dev);
		ret = 0;
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}

static int helioscom_char_close(struct inode *inode, struct file *file)
{
	int ret;

	mutex_lock(&helios_char_mutex);
	ret = helioscom_close(&handle);
	device_open = 0;
	mutex_unlock(&helios_char_mutex);
	return ret;
}

static int helios_daemon_probe(struct platform_device *pdev)
{
	struct device_node *node;
	int rc = 0;

	node = pdev->dev.of_node;

	dev = kzalloc(sizeof(struct heliosdaemon_priv), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	/* Add wake lock for PM suspend */
	dev->helioscom_ws = wakeup_source_register(&pdev->dev, "Slatcom_wake_lock");
	dev->helioscom_current_state = HELIOSCOM_STATE_UNKNOWN;
	//rc = helioscom_rpmsg_init(dev);
	if (rc)
		return -ENODEV;
	dev->platform_dev = &pdev->dev;
	pr_info("%s success\n", __func__);
	helios_pdev = pdev;
	ssr_register();
	return 0;
}

static const struct of_device_id helios_daemon_of_match[] = {
	{ .compatible = "qcom,helios-daemon", },
	{ }
};
MODULE_DEVICE_TABLE(of, helios_daemon_of_match);

static struct platform_driver helios_daemon_driver = {
	.probe  = helios_daemon_probe,
	.driver = {
		.name = "helios-daemon",
		.of_match_table = helios_daemon_of_match,
	},
};

static const struct file_operations fops = {
	.owner          = THIS_MODULE,
	.open           = helioscom_char_open,
	.release        = helioscom_char_close,
	.unlocked_ioctl = helios_com_ioctl,
};

/**
 *ssr_helios_cb(): callback function is called
 *by ssr framework when HELIOS goes down, up and during ramdump
 *collection. It handles HELIOS shutdown and power up events.
 */
static int ssr_helios_cb(struct notifier_block *this,
		unsigned long opcode, void *data)
{
	struct helios_event heliose;

	switch (opcode) {
	case QCOM_SSR_BEFORE_SHUTDOWN:
		pr_err("Helios before shutdown\n");
		heliose.e_type = HELIOS_BEFORE_POWER_DOWN;
		helioscom_set_spi_state(HELIOSCOM_SPI_BUSY);
		send_uevent(&heliose);
		break;
	case QCOM_SSR_AFTER_SHUTDOWN:
		pr_err("Helios after shutdown\n");
		heliose.e_type = HELIOS_AFTER_POWER_DOWN;
		helioscom_heliosdown_handler();
		send_uevent(&heliose);
		break;
	case QCOM_SSR_BEFORE_POWERUP:
		pr_err("Helios before powerup\n");
		heliose.e_type = HELIOS_BEFORE_POWER_UP;
		send_uevent(&heliose);
		break;
	case QCOM_SSR_AFTER_POWERUP:
		pr_err("Helios after powerup\n");
		heliose.e_type = HELIOS_AFTER_POWER_UP;
		helioscom_set_spi_state(HELIOSCOM_SPI_FREE);
		send_uevent(&heliose);
		break;
	}
	return NOTIFY_DONE;
}


bool is_helios_running(void)
{
	if (helios_app_running) {
		helios_app_running = false;
		return true;
	}
	return false;
}
EXPORT_SYMBOL(is_helios_running);

static struct notifier_block ssr_helios_nb = {
	.notifier_call = ssr_helios_cb,
	.priority = 0,
};

static struct service_info service_data[1] = {
	{
		.name = "SSR_HELIOS",
		.domain_id = SSR_DOMAIN_HELIOS,
		.nb = &ssr_helios_nb,
		.handle = NULL,
	},
};

/**
 * ssr_register checks that domain id should be in range and register
 * SSR framework for value at domain id.
 */
static void ssr_register(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(service_data); i++) {
		if ((service_data[i].domain_id < 0) ||
				(service_data[i].domain_id >= SSR_DOMAIN_MAX)) {
			pr_err("Invalid service ID = %d\n",
					service_data[i].domain_id);
		} else {
			service_data[i].handle =
					qcom_register_ssr_notifier(
					ssr_domains[service_data[i].domain_id],
					service_data[i].nb);
			if (IS_ERR_OR_NULL(service_data[i].handle)) {
				pr_err("ssr register failed for id = %d\n",
						service_data[i].domain_id);
				service_data[i].handle = NULL;
			}
		}
	}

}

static int __init init_helios_com_dev(void)
{
	int ret, i;

	ret = alloc_chrdev_region(&helios_dev, 0, 1, HELIOSCOM);
	if (ret  < 0) {
		pr_err("failed with error %d\n", ret);
		return ret;
	}
	cdev_init(&helios_cdev, &fops);

	ret = cdev_add(&helios_cdev, helios_dev, 1);
	if (ret < 0) {
		unregister_chrdev_region(helios_dev, 1);
		pr_err("device registration failed\n");
		return ret;
	}
	helios_class = class_create(THIS_MODULE, HELIOSCOM);
	if (IS_ERR_OR_NULL(helios_class)) {
		cdev_del(&helios_cdev);
		unregister_chrdev_region(helios_dev, 1);
		pr_err("class creation failed\n");
		return PTR_ERR(helios_class);
	}

	dev_ret = device_create(helios_class, NULL, helios_dev, NULL, HELIOSCOM);
	if (IS_ERR_OR_NULL(dev_ret)) {
		class_destroy(helios_class);
		cdev_del(&helios_cdev);
		unregister_chrdev_region(helios_dev, 1);
		pr_err("device create failed\n");
		return PTR_ERR(dev_ret);
	}

	ret = class_register(&helioscom_intf_class);
	if (ret < 0) {
		pr_err("Failed to register helioscom_intf_class rc=%d\n", ret);
		return ret;
	}

	for (i = 0; i < HELIOSCOM_INTF_N_FILES; i++) {
		if (class_create_file(&helioscom_intf_class, &helioscom_attr[i]))
			pr_err("%s: failed to create helios-bt/dsp entry\n", __func__);
	}

	if (platform_driver_register(&helios_daemon_driver))
		pr_err("%s: failed to register helios-daemon register\n", __func__);

	return 0;
}

static void __exit exit_helios_com_dev(void)
{
	int i;

	device_destroy(helios_class, helios_dev);
	class_destroy(helios_class);
	for (i = 0; i < HELIOSCOM_INTF_N_FILES; i++)
		class_remove_file(&helioscom_intf_class, &helioscom_attr[i]);
	class_unregister(&helioscom_intf_class);
	cdev_del(&helios_cdev);
	unregister_chrdev_region(helios_dev, 1);
	platform_driver_unregister(&helios_daemon_driver);
}

module_init(init_helios_com_dev);
module_exit(exit_helios_com_dev);
MODULE_LICENSE("GPL v2");
