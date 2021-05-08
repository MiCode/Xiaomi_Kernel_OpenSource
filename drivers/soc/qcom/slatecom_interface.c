// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */
#define pr_fmt(msg) "slatecom_dev:" msg

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
#include "slatecom.h"
#include "linux/slatecom_interface.h"
#include "slatecom_interface.h"
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <asm/dma.h>
#include <linux/dma-mapping.h>

#include "peripheral-loader.h"
#include "../../misc/qseecom_kernel.h"
//#include "pil_slate_intf.h"

#define SLATECOM "slate_com_dev"

#define SLATEDAEMON_LDO09_LPM_VTG 0
#define SLATEDAEMON_LDO09_NPM_VTG 10000

#define SLATEDAEMON_LDO03_LPM_VTG 0
#define SLATEDAEMON_LDO03_NPM_VTG 10000

#define MPPS_DOWN_EVENT_TO_SLATE_TIMEOUT 3000
#define ADSP_DOWN_EVENT_TO_SLATE_TIMEOUT 3000
#define MAX_APP_NAME_SIZE 100

/*pil_slate_intf.h*/
#define RESULT_SUCCESS 0
#define RESULT_FAILURE -1

#define SLATECOM_INTF_N_FILES 2
#define BUF_SIZE 10

static char btss_state[BUF_SIZE] = "offline";
static char dspss_state[BUF_SIZE] = "offline";

/* tzapp command list.*/
enum slate_tz_commands {
	SLATEPIL_RAMDUMP,
	SLATEPIL_IMAGE_LOAD,
	SLATEPIL_AUTH_MDT,
	SLATEPIL_DLOAD_CONT,
	SLATEPIL_GET_SLATE_VERSION,
	SLATEPIL_TWM_DATA,
};

/* tzapp slate request.*/
struct tzapp_slate_req {
	uint8_t tzapp_slate_cmd;
	uint8_t padding[3];
	phys_addr_t address_fw;
	size_t size_fw;
} __attribute__ ((__packed__));

/* tzapp slate response.*/
struct tzapp_slate_rsp {
	uint32_t tzapp_slate_cmd;
	uint32_t slate_info_len;
	int32_t status;
	uint32_t slate_info[100];
} __attribute__ ((__packed__));

enum {
	SSR_DOMAIN_SLATE,
	SSR_DOMAIN_MODEM,
	SSR_DOMAIN_ADSP,
	SSR_DOMAIN_MAX,
};

struct slatedaemon_priv {
	void *pil_h;
	struct qseecom_handle *qseecom_handle;
	int app_status;
	unsigned long attrs;
	u32 cmd_status;
	struct device *platform_dev;
};

struct slate_event {
	enum slate_event_type e_type;
};

struct service_info {
	const char                      name[32];
	int                             domain_id;
	void                            *handle;
	struct notifier_block           *nb;
};

static char *ssr_domains[] = {
	"slate-wear",
	"modem",
	"adsp",
};

static struct slatedaemon_priv *dev;
static unsigned int slatereset_gpio;
static  DEFINE_MUTEX(slate_char_mutex);
static  struct cdev              slate_cdev;
static  struct class             *slate_class;
struct  device                   *dev_ret;
static  dev_t                    slate_dev;
static  int                      device_open;
static  void                     *handle;
static	bool                     twm_exit;
static	bool                     slate_app_running;
static	bool                     slate_dsp_error;
static	bool                     slate_bt_error;
static  struct   slatecom_open_config_type   config_type;
static DECLARE_COMPLETION(slate_modem_down_wait);
static DECLARE_COMPLETION(slate_adsp_down_wait);

static ssize_t slate_bt_state_sysfs_read
			(struct class *class, struct class_attribute *attr, char *buf)
{
	pr_debug("In %s\n", __func__);
	return scnprintf(buf, BUF_SIZE, btss_state);
}

static ssize_t slate_dsp_state_sysfs_read
			(struct class *class, struct class_attribute *attr, char *buf)
{
	pr_debug("In %s\n", __func__);
	return	scnprintf(buf, BUF_SIZE, dspss_state);
}

struct class_attribute slatecom_attr[] = {
	{
		.attr = {
			.name = "slate_bt_state",
			.mode = 0644
		},
		.show	= slate_bt_state_sysfs_read,
	},
	{
		.attr = {
			.name = "slate_dsp_state",
			.mode = 0644
		},
		.show	= slate_dsp_state_sysfs_read,
	},
};
struct class slatecom_intf_class = {
	.name = "slatecom"
};

/**
 * send_uevent(): send events to user space
 * pce : ssr event handle value
 * Return: 0 on success, standard Linux error code on error
 *
 * It adds pce value to event and broadcasts to user space.
 */
static int send_uevent(struct slate_event *pce)
{
	char event_string[32];
	char *envp[2] = { event_string, NULL };

	snprintf(event_string, ARRAY_SIZE(event_string),
			"SLATE_EVENT=%d", pce->e_type);
	return kobject_uevent_env(&dev_ret->kobj, KOBJ_CHANGE, envp);
}

static int slatecom_char_open(struct inode *inode, struct file *file)
{
	int ret;

	mutex_lock(&slate_char_mutex);
	if (device_open == 1) {
		pr_err("device is already open\n");
		mutex_unlock(&slate_char_mutex);
		return -EBUSY;
	}
	device_open++;
	handle = slatecom_open(&config_type);
	mutex_unlock(&slate_char_mutex);
	if (IS_ERR(handle)) {
		device_open = 0;
		ret = PTR_ERR(handle);
		handle = NULL;
		return ret;
	}
	return 0;
}

static int slatechar_read_cmd(struct slate_ui_data *fui_obj_msg,
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
		ret = slatecom_reg_read(handle, fui_obj_msg->cmd,
				fui_obj_msg->num_of_words,
				read_buf);
		break;
	case AHB_READ:
		ret = slatecom_ahb_read(handle,
				fui_obj_msg->slate_address,
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

static int slatechar_write_cmd(struct slate_ui_data *fui_obj_msg, unsigned int type)
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
		ret = slatecom_reg_write(handle, fui_obj_msg->cmd,
				fui_obj_msg->num_of_words,
				write_buf);
		break;
	case AHB_WRITE:
		ret = slatecom_ahb_write(handle,
				fui_obj_msg->slate_address,
				fui_obj_msg->num_of_words,
				write_buf);
		break;
	}
	kfree(write_buf);
	return ret;
}

int slate_soft_reset(void)
{
	pr_debug("do SLATE reset using gpio %d\n", slatereset_gpio);
	if (!gpio_is_valid(slatereset_gpio)) {
		pr_err("gpio %d is not valid\n", slatereset_gpio);
		return -ENXIO;
	}
	if (gpio_direction_output(slatereset_gpio, 1))
		pr_err("gpio %d direction not set\n", slatereset_gpio);

	/* Sleep for 50ms for hardware to detect signal as high */
	msleep(50);

	gpio_set_value(slatereset_gpio, 0);

	/* Sleep for 50ms for hardware to detect signal as high */
	msleep(50);
	gpio_set_value(slatereset_gpio, 1);

	return 0;
}
EXPORT_SYMBOL(slate_soft_reset);

static int modem_down2_slate(void)
{
	complete(&slate_modem_down_wait);
	return 0;
}

static int adsp_down2_slate(void)
{
	complete(&slate_adsp_down_wait);
	return 0;
}

static long slate_com_ioctl(struct file *filp,
		unsigned int ui_slatecom_cmd, unsigned long arg)
{
	int ret;
	struct slate_ui_data ui_obj_msg;

	switch (ui_slatecom_cmd) {
	case REG_READ:
	case AHB_READ:
		if (copy_from_user(&ui_obj_msg, (void __user *) arg,
				sizeof(ui_obj_msg))) {
			pr_err("The copy from user failed\n");
			ret = -EFAULT;
		}
		ret = slatechar_read_cmd(&ui_obj_msg,
				ui_slatecom_cmd);
		if (ret < 0)
			pr_err("slatechar_read_cmd failed\n");
		break;
	case AHB_WRITE:
	case REG_WRITE:
		if (copy_from_user(&ui_obj_msg, (void __user *) arg,
				sizeof(ui_obj_msg))) {
			pr_err("The copy from user failed\n");
			ret = -EFAULT;
		}
		ret = slatechar_write_cmd(&ui_obj_msg, ui_slatecom_cmd);
		if (ret < 0)
			pr_err("slatechar_write_cmd failed\n");
		break;
	case SET_SPI_FREE:
		ret = slatecom_set_spi_state(SLATECOM_SPI_FREE);
		break;
	case SET_SPI_BUSY:
		ret = slatecom_set_spi_state(SLATECOM_SPI_BUSY);
		break;
	case SLATE_SOFT_RESET:
		ret = slate_soft_reset();
		break;
	case SLATE_MODEM_DOWN2_SLATE_DONE:
		ret = modem_down2_slate();
		break;
	case SLATE_ADSP_DOWN2_SLATE_DONE:
		ret = adsp_down2_slate();
		break;
	case SLATE_TWM_EXIT:
		twm_exit = true;
		ret = 0;
		break;
	case SLATE_APP_RUNNING:
		slate_app_running = true;
		ret = 0;
		break;
	case SLATE_WEAR_LOAD:
		ret = 0;
		if (dev->pil_h) {
			pr_err("slate-wear is already loaded\n");
			ret = -EFAULT;
			break;
		}
		dev->pil_h = subsystem_get_with_fwname("slate-wear", "slate-wear");
		if (!dev->pil_h) {
			pr_err("failed to load slate-wear\n");
			ret = -EFAULT;
		}
		break;
	case SLATE_WEAR_UNLOAD:
		if (dev->pil_h) {
			subsystem_put(dev->pil_h);
			dev->pil_h = NULL;
			slate_soft_reset();
		}
		ret = 0;
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}

static int slatecom_char_close(struct inode *inode, struct file *file)
{
	int ret;

	mutex_lock(&slate_char_mutex);
	ret = slatecom_close(&handle);
	device_open = 0;
	mutex_unlock(&slate_char_mutex);
	return ret;
}

static int slate_daemon_probe(struct platform_device *pdev)
{
	struct device_node *node;

	node = pdev->dev.of_node;

	dev = kzalloc(sizeof(struct slatedaemon_priv), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->platform_dev = &pdev->dev;
	pr_info("%s success\n", __func__);

	return 0;
}

static const struct of_device_id slate_daemon_of_match[] = {
	{ .compatible = "qcom,slate-daemon", },
	{ }
};
MODULE_DEVICE_TABLE(of, slate_daemon_of_match);

static struct platform_driver slate_daemon_driver = {
	.probe  = slate_daemon_probe,
	.driver = {
		.name = "slate-daemon",
		.of_match_table = slate_daemon_of_match,
	},
};

static const struct file_operations fops = {
	.owner          = THIS_MODULE,
	.open           = slatecom_char_open,
	.release        = slatecom_char_close,
	.unlocked_ioctl = slate_com_ioctl,
};

/**
 *ssr_slate_cb(): callback function is called
 *by ssr framework when SLATE goes down, up and during ramdump
 *collection. It handles SLATE shutdown and power up events.
 */
static int ssr_slate_cb(struct notifier_block *this,
		unsigned long opcode, void *data)
{
	struct slate_event slatee;

	switch (opcode) {
	case SUBSYS_BEFORE_SHUTDOWN:
		pr_debug("Slate before shutdown\n");
		slatee.e_type = SLATE_BEFORE_POWER_DOWN;
		slatecom_slatedown_handler();
		slatecom_set_spi_state(SLATECOM_SPI_BUSY);
		send_uevent(&slatee);
		break;
	case SUBSYS_AFTER_SHUTDOWN:
		pr_debug("Slate after shutdown\n");
		slatee.e_type = SLATE_AFTER_POWER_DOWN;
		slatecom_slatedown_handler();
		send_uevent(&slatee);
		break;
	case SUBSYS_BEFORE_POWERUP:
		pr_debug("Slate before powerup\n");
		slatee.e_type = SLATE_BEFORE_POWER_UP;
		slatecom_slatedown_handler();
		send_uevent(&slatee);
	break;
	case SUBSYS_AFTER_POWERUP:
		pr_debug("Slate after powerup\n");
		slatecom_set_spi_state(SLATECOM_SPI_FREE);
		send_uevent(&slatee);
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
	struct slate_event modeme;
	int ret;

	switch (opcode) {
	case SUBSYS_BEFORE_SHUTDOWN:
		modeme.e_type = MODEM_BEFORE_POWER_DOWN;
		reinit_completion(&slate_modem_down_wait);
		send_uevent(&modeme);
		ret = wait_for_completion_timeout(&slate_modem_down_wait,
			msecs_to_jiffies(MPPS_DOWN_EVENT_TO_SLATE_TIMEOUT));
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
	struct slate_event adspe;
	int ret;

	switch (opcode) {
	case SUBSYS_BEFORE_SHUTDOWN:
		adspe.e_type = ADSP_BEFORE_POWER_DOWN;
		reinit_completion(&slate_adsp_down_wait);
		send_uevent(&adspe);
		ret = wait_for_completion_timeout(&slate_adsp_down_wait,
			msecs_to_jiffies(ADSP_DOWN_EVENT_TO_SLATE_TIMEOUT));
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

bool is_slate_running(void)
{
	if (slate_app_running) {
		slate_app_running = false;
		return true;
	}
	return false;
}
EXPORT_SYMBOL(is_slate_running);

void set_slate_dsp_state(bool status)
{
	struct slate_event statee;

	slate_dsp_error = status;
	if (!status) {
		statee.e_type = SLATE_DSP_ERROR;
		strlcpy(dspss_state, "error", BUF_SIZE);
	} else {
		statee.e_type = SLATE_DSP_READY;
		strlcpy(dspss_state, "ready", BUF_SIZE);
	}
	send_uevent(&statee);
}
EXPORT_SYMBOL(set_slate_dsp_state);

void set_slate_bt_state(bool status)
{
	struct slate_event statee;

	slate_bt_error = status;
	if (!status) {
		statee.e_type = SLATE_BT_ERROR;
		strlcpy(btss_state, "error", BUF_SIZE);
	} else {
		statee.e_type = SLATE_BT_READY;
		strlcpy(btss_state, "ready", BUF_SIZE);
	}
	send_uevent(&statee);
}
EXPORT_SYMBOL(set_slate_bt_state);

static struct notifier_block ssr_modem_nb = {
	.notifier_call = ssr_modem_cb,
	.priority = 0,
};

static struct notifier_block ssr_adsp_nb = {
	.notifier_call = ssr_adsp_cb,
	.priority = 0,
};

static struct notifier_block ssr_slate_nb = {
	.notifier_call = ssr_slate_cb,
	.priority = 0,
};

static struct service_info service_data[3] = {
	{
		.name = "SSR_SLATE",
		.domain_id = SSR_DOMAIN_SLATE,
		.nb = &ssr_slate_nb,
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
					subsys_notif_register_notifier(
					ssr_domains[service_data[i].domain_id],
					service_data[i].nb);
			if (IS_ERR_OR_NULL(service_data[i].handle)) {
				pr_err("subsys register failed for id = %d\n",
						service_data[i].domain_id);
				service_data[i].handle = NULL;
			}
		}
	}

}

static int __init init_slate_com_dev(void)
{
	int ret, i;

	ret = alloc_chrdev_region(&slate_dev, 0, 1, SLATECOM);
	if (ret  < 0) {
		pr_err("failed with error %d\n", ret);
		return ret;
	}
	cdev_init(&slate_cdev, &fops);

	ret = cdev_add(&slate_cdev, slate_dev, 1);
	if (ret < 0) {
		unregister_chrdev_region(slate_dev, 1);
		pr_err("device registration failed\n");
		return ret;
	}
	slate_class = class_create(THIS_MODULE, SLATECOM);
	if (IS_ERR_OR_NULL(slate_class)) {
		cdev_del(&slate_cdev);
		unregister_chrdev_region(slate_dev, 1);
		pr_err("class creation failed\n");
		return PTR_ERR(slate_class);
	}

	dev_ret = device_create(slate_class, NULL, slate_dev, NULL, SLATECOM);
	if (IS_ERR_OR_NULL(dev_ret)) {
		class_destroy(slate_class);
		cdev_del(&slate_cdev);
		unregister_chrdev_region(slate_dev, 1);
		pr_err("device create failed\n");
		return PTR_ERR(dev_ret);
	}

	ret = class_register(&slatecom_intf_class);
	if (ret < 0) {
		pr_err("Failed to register slatecom_intf_class rc=%d\n", ret);
		return ret;
	}

	for (i = 0; i < SLATECOM_INTF_N_FILES; i++) {
		if (class_create_file(&slatecom_intf_class, &slatecom_attr[i]))
			pr_err("%s: failed to create slate-bt/dsp entry\n", __func__);
	}

	if (platform_driver_register(&slate_daemon_driver))
		pr_err("%s: failed to register slate-daemon register\n", __func__);

	ssr_register();

	return 0;
}

static void __exit exit_slate_com_dev(void)
{
	int i;
	device_destroy(slate_class, slate_dev);
	class_destroy(slate_class);
	for (i = 0; i < SLATECOM_INTF_N_FILES; i++)
		class_remove_file(&slatecom_intf_class, &slatecom_attr[i]);
	class_unregister(&slatecom_intf_class);
	cdev_del(&slate_cdev);
	unregister_chrdev_region(slate_dev, 1);
	platform_driver_unregister(&slate_daemon_driver);
}

module_init(init_slate_com_dev);
module_exit(exit_slate_com_dev);
MODULE_LICENSE("GPL v2");
