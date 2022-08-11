// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "slatecom_rpmsg.h"

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
} __packed;

/* tzapp slate response.*/
struct tzapp_slate_rsp {
	uint32_t tzapp_slate_cmd;
	uint32_t slate_info_len;
	int32_t status;
	uint32_t slate_info[100];
} __packed;

enum {
	SSR_DOMAIN_SLATE,
	SSR_DOMAIN_MODEM,
	SSR_DOMAIN_ADSP,
	SSR_DOMAIN_MAX,
};

enum slatecom_state {
	SLATECOM_STATE_UNKNOWN,
	SLATECOM_STATE_INIT,
	SLATECOM_STATE_GLINK_OPEN,
	SLATECOM_STATE_SLATE_SSR
};

struct slatedaemon_priv {
	void *pil_h;
	int app_status;
	unsigned long attrs;
	u32 cmd_status;
	struct device *platform_dev;
	bool slatecom_rpmsg;
	bool slate_resp_cmplt;
	void *lhndl;
	wait_queue_head_t link_state_wait;
	char rx_buf[20];
	struct work_struct slatecom_up_work;
	struct work_struct slatecom_down_work;
	struct mutex glink_mutex;
	struct mutex slatecom_state_mutex;
	enum slatecom_state slatecom_current_state;
	struct workqueue_struct *slatecom_wq;
	struct wakeup_source slatecom_ws;
};

static void *slatecom_intf_drv;

struct slate_event {
	enum slate_event_type e_type;
};

struct service_info {
	const char                      name[32];
	int                             domain_id;
	void                            *handle;
	struct notifier_block           *nb;
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
static struct srcu_notifier_head slatecom_notifier_chain;

static ssize_t slate_bt_state_sysfs_read
			(struct class *class, struct class_attribute *attr, char *buf)
{
	return scnprintf(buf, BUF_SIZE, btss_state);
}

static ssize_t slate_dsp_state_sysfs_read
			(struct class *class, struct class_attribute *attr, char *buf)
{
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

void slatecom_intf_notify_glink_channel_state(bool state)
{
	struct slatedaemon_priv *dev =
		container_of(slatecom_intf_drv, struct slatedaemon_priv, lhndl);

	pr_debug("%s: slate_ctrl channel state: %d\n", __func__, state);
	dev->slatecom_rpmsg = state;
}
EXPORT_SYMBOL(slatecom_intf_notify_glink_channel_state);

void slatecom_rx_msg(void *data, int len)
{
	struct slatedaemon_priv *dev =
		container_of(slatecom_intf_drv, struct slatedaemon_priv, lhndl);

	dev->slate_resp_cmplt = true;
	wake_up(&dev->link_state_wait);
	memcpy(dev->rx_buf, data, len);
}
EXPORT_SYMBOL(slatecom_rx_msg);

static int slatecom_tx_msg(struct slatedaemon_priv *dev, void  *msg, size_t len)
{
	int rc = 0;
	uint8_t resp = 0;

	mutex_lock(&dev->glink_mutex);
	__pm_stay_awake(&dev->slatecom_ws);
	if (!dev->slatecom_rpmsg) {
		pr_err("slatecom-rpmsg is not probed yet, waiting for it to be probed\n");
		goto err_ret;
	}
	rc = slatecom_rpmsg_tx_msg(msg, len);

	/* wait for sending command to SLATE */
	rc = wait_event_timeout(dev->link_state_wait,
			(rc == 0), msecs_to_jiffies(TIMEOUT_MS));
	if (rc == 0) {
		pr_err("failed to send command to SLATE %d\n", rc);
		goto err_ret;
	}

	/* wait for getting response from SLATE */
	rc = wait_event_timeout(dev->link_state_wait,
			dev->slate_resp_cmplt,
				 msecs_to_jiffies(TIMEOUT_MS));
	if (rc == 0) {
		pr_err("failed to get SLATE response %d\n", rc);
		goto err_ret;
	}
	dev->slate_resp_cmplt = false;
	/* check SLATE response */
	resp = *(uint8_t *)dev->rx_buf;
	if (resp == 0x01) {
		pr_err("Bad SLATE response\n");
		rc = -EINVAL;
		goto err_ret;
	}
	rc = 0;

err_ret:
	__pm_relax(&dev->slatecom_ws);
	mutex_unlock(&dev->glink_mutex);
	return rc;
}

/**
 * send_state_change_cmd send state transition event to Slate
 * and wait for the response.
 * The response is returned to the caller.
 */
static int send_state_change_cmd(struct slate_ui_data *ui_obj_msg)
{
	int ret = 0;
	struct msg_header_t msg_header = {0, 0};
	struct slatedaemon_priv *dev = container_of(slatecom_intf_drv,
					struct slatedaemon_priv,
					lhndl);
	uint32_t state = ui_obj_msg->cmd;

	switch (state) {
	case STATE_TWM_ENTER:
		msg_header.opcode = GMI_MGR_ENTER_TWM;
		break;
	case STATE_DS_ENTER:
		msg_header.opcode = GMI_MGR_ENTER_TRACKER_DS;
		break;
	case STATE_DS_EXIT:
		msg_header.opcode = GMI_MGR_EXIT_TRACKER_DS;
		break;
	case STATE_S2D_ENTER:
		msg_header.opcode = GMI_MGR_ENTER_TRACKER_DS;
		break;
	case STATE_S2D_EXIT:
		msg_header.opcode = GMI_MGR_EXIT_TRACKER_DS;
		break;
	default:
		pr_err("Invalid MSM State transtion cmd\n");
		break;
	}
	ret = slatecom_tx_msg(dev, &msg_header.opcode, sizeof(msg_header.opcode));
	if (ret < 0)
		pr_err("MSM State transtion event cmd failed\n");
	return ret;
}

static int slatecom_char_open(struct inode *inode, struct file *file)
{
	int ret = 0;

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
	int               ret = 0;
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

int send_wlan_state(enum WMSlateCtrlChnlOpcode type)
{
	int ret = 0;
	struct msg_header_t msg_header = {0, 0};
	struct slatedaemon_priv *dev = container_of(slatecom_intf_drv,
					struct slatedaemon_priv,
					lhndl);

	switch (type) {
	case GMI_MGR_WLAN_BOOT_INIT:
		msg_header.opcode = GMI_MGR_WLAN_BOOT_INIT;
		break;
	case GMI_MGR_WLAN_BOOT_COMPLETE:
		msg_header.opcode = GMI_MGR_WLAN_BOOT_COMPLETE;
		break;
	case GMI_WLAN_5G_CONNECT:
		msg_header.opcode = GMI_WLAN_5G_CONNECT;
		break;
	case GMI_WLAN_5G_DISCONNECT:
		msg_header.opcode = GMI_WLAN_5G_DISCONNECT;
		break;
	default:
		pr_err("Invalid WLAN State transtion cmd = %d\n", type);
		break;
	}

	ret = slatecom_tx_msg(dev, &msg_header.opcode, sizeof(msg_header.opcode));
	if (ret < 0)
		pr_err("WLAN State transtion event cmd failed with = %d\n", ret);
	return ret;
}
EXPORT_SYMBOL(send_wlan_state);

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

static int send_time_sync(struct slate_ui_data *tui_obj_msg)
{
	int ret = 0;
	void *write_buf;

	write_buf = kmalloc_array(tui_obj_msg->num_of_words, sizeof(uint32_t),
							GFP_KERNEL);
	if (write_buf == NULL)
		return -ENOMEM;
	write_buf = memdup_user(tui_obj_msg->buffer,
					tui_obj_msg->num_of_words * sizeof(uint32_t));
	if (IS_ERR(write_buf)) {
		ret = PTR_ERR(write_buf);
		kfree(write_buf);
		return ret;
	}
	ret = slatecom_tx_msg(dev, write_buf, tui_obj_msg->num_of_words*4);
	if (ret < 0)
		pr_err("send_time_data cmd failed\n");
	else
		pr_info("send_time_data cmd success\n");
return ret;
}

static int send_debug_config(struct slate_ui_data *tui_obj_msg)
{
	int ret = 0;
	struct msg_header_t msg_header = {0, 0};
	struct slatedaemon_priv *dev = container_of(slatecom_intf_drv,
					struct slatedaemon_priv,
					lhndl);
	uint32_t config = tui_obj_msg->cmd;

	switch (config) {
	case ENABLE_PMIC_RTC:
		msg_header.opcode = GMI_MGR_ENABLE_PMIC_RTC;
		break;
	case DISABLE_PMIC_RTC:
		msg_header.opcode = GMI_MGR_DISABLE_PMIC_RTC;
		break;
	case ENABLE_QCLI:
		msg_header.opcode = GMI_MGR_ENABLE_QCLI;
		break;
	case DISABLE_QCLI:
		msg_header.opcode = GMI_MGR_DISABLE_QCLI;
		break;
	default:
		pr_err("Invalid debug config cmd\n");
		return -EINVAL;
	}
	ret = slatecom_tx_msg(dev, &msg_header.opcode, sizeof(msg_header.opcode));

	if (ret < 0)
		pr_err("failed to send debug config cmd\n");
	return ret;
}

static long slate_com_ioctl(struct file *filp,
		unsigned int ui_slatecom_cmd, unsigned long arg)
{
	int ret = 0;
	struct slate_ui_data ui_obj_msg;

	if (filp == NULL)
		return -EINVAL;

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
	case SLATE_LOAD:
		pr_err("cmd SLATE_LOAD depricated\n");
		break;
	case SLATE_UNLOAD:
		pr_err("cmd SLATE_UNLOAD depricated\n");
		break;
	case DEVICE_STATE_TRANSITION:
		if (dev->slatecom_current_state != SLATECOM_STATE_GLINK_OPEN) {
			pr_err("driver not ready, glink is not open\n");
			return -ENODEV;
		}
		if (copy_from_user(&ui_obj_msg, (void __user *)arg,
					sizeof(ui_obj_msg))) {
			pr_err("The copy from user failed-state transition\n");
			ret = -EFAULT;
		}
		ret = send_state_change_cmd(&ui_obj_msg);
		if (ret < 0)
			pr_err("device_state_transition cmd failed\n");
		break;
	case SEND_TIME_DATA:
		if (dev->slatecom_current_state != SLATECOM_STATE_GLINK_OPEN) {
			pr_err("%s: driver not ready, current state: %d\n",
			__func__, dev->slatecom_current_state);
			return -ENODEV;
		}
		if (copy_from_user(&ui_obj_msg, (void __user *) arg,
					sizeof(ui_obj_msg))) {
			pr_err("The copy from user failed for time data\n");
			ret = -EFAULT;
		}
		ret = send_time_sync(&ui_obj_msg);
		if (ret < 0)
			pr_err("send_time_data cmd failed\n");
		break;
	case SEND_DEBUG_CONFIG:
		if (dev->slatecom_current_state != SLATECOM_STATE_GLINK_OPEN) {
			pr_err("%s: driver not ready, current state: %d\n",
			__func__, dev->slatecom_current_state);
			return -ENODEV;
		}
		if (copy_from_user(&ui_obj_msg, (void __user *) arg,
					sizeof(ui_obj_msg))) {
			pr_err("The copy from user failed for time data\n");
			ret = -EFAULT;
		}
		ret = send_debug_config(&ui_obj_msg);
		if (ret < 0)
			pr_err("send_time_data cmd failed\n");
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}

static ssize_t slatecom_char_write(struct file *f, const char __user *buf,
				size_t count, loff_t *off)
{
	unsigned char qcli_cmnd;
	uint32_t opcode;
	int ret = 0;
	struct slatedaemon_priv *dev = container_of(slatecom_intf_drv,
					struct slatedaemon_priv,
					lhndl);

	if (copy_from_user(&qcli_cmnd, buf, sizeof(unsigned char)))
		return -EFAULT;

	pr_debug("%s: QCLI command arg = %c\n", __func__, qcli_cmnd);

	switch (qcli_cmnd) {
	case '0':
		opcode = GMI_MGR_DISABLE_QCLI;
		ret = slatecom_tx_msg(dev, &opcode, sizeof(opcode));
		if (ret < 0)
			pr_err("MSM QCLI Disable cmd failed\n");
		break;
	case '1':
		opcode = GMI_MGR_ENABLE_QCLI;
		ret = slatecom_tx_msg(dev, &opcode, sizeof(opcode));
		if (ret < 0)
			pr_err("MSM QCLI Enable cmd failed\n");
		break;
	case '2':
		pr_err("subsystem start notify, cmd depricated\n");
		break;
	case '3':
		pr_err("subsystem stop notify, cmd depricated\n");
		break;
	case '4':
		opcode = GMI_MGR_ENABLE_PMIC_RTC;
		ret = slatecom_tx_msg(dev, &opcode, sizeof(opcode));
		if (ret < 0)
			pr_err("MSM RTC Enable cmd failed\n");
		break;
	case '5':
		opcode = GMI_MGR_DISABLE_PMIC_RTC;
		ret = slatecom_tx_msg(dev, &opcode, sizeof(opcode));
		if (ret < 0)
			pr_err("MSM RTC Disable cmd failed\n");
		break;

	default:
		pr_err("MSM QCLI Invalid Option\n");
		break;
	}

	*off += count;
	return count;
}

static int slatecom_char_close(struct inode *inode, struct file *file)
{
	int ret = 0;

	mutex_lock(&slate_char_mutex);
	ret = slatecom_close(&handle);
	device_open = 0;
	mutex_unlock(&slate_char_mutex);
	return ret;
}

static void slatecom_slateup_work(struct work_struct *work)
{
	int ret = 0;
	struct slatedaemon_priv *dev =
			container_of(work, struct slatedaemon_priv, slatecom_up_work);

	mutex_lock(&dev->slatecom_state_mutex);
	if (!dev->slatecom_rpmsg)
		pr_err("slatecom-rpmsg is not probed yet\n");
	ret = wait_event_timeout(dev->link_state_wait,
				dev->slatecom_rpmsg, msecs_to_jiffies(TIMEOUT_MS));
	if (ret == 0) {
		pr_err("channel connection time out %d\n", ret);
		goto glink_err;
	}
	dev->slatecom_current_state = SLATECOM_STATE_GLINK_OPEN;
	goto unlock;

glink_err:
	dev->slatecom_current_state = SLATECOM_STATE_INIT;
unlock:
	mutex_unlock(&dev->slatecom_state_mutex);
}


static void slatecom_slatedown_work(struct work_struct *work)
{
	struct slatedaemon_priv *dev = container_of(work, struct slatedaemon_priv,
								slatecom_down_work);

	mutex_lock(&dev->slatecom_state_mutex);

	pr_debug("Slatecom current state is : %d\n", dev->slatecom_current_state);

	dev->slatecom_current_state = SLATECOM_STATE_SLATE_SSR;

	mutex_unlock(&dev->slatecom_state_mutex);
}

static int slatecom_rpmsg_init(struct slatedaemon_priv *dev)
{
	slatecom_intf_drv = &dev->lhndl;
	mutex_init(&dev->glink_mutex);
	mutex_init(&dev->slatecom_state_mutex);

	dev->slatecom_wq =
		create_singlethread_workqueue("slatecom-work-queue");
	if (!dev->slatecom_wq) {
		pr_err("Failed to init Slatecom work-queue\n");
		return -ENOMEM;
	}

	init_waitqueue_head(&dev->link_state_wait);

	/* set default slatecom state */
	dev->slatecom_current_state = SLATECOM_STATE_INIT;

	/* Init all works */
	INIT_WORK(&dev->slatecom_up_work, slatecom_slateup_work);
	INIT_WORK(&dev->slatecom_down_work, slatecom_slatedown_work);

	return 0;
}

static int slate_daemon_probe(struct platform_device *pdev)
{
	struct device_node *node;
	int rc = 0;

	node = pdev->dev.of_node;

	dev = kzalloc(sizeof(struct slatedaemon_priv), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	/* Add wake lock for PM suspend */
	wakeup_source_add(&dev->slatecom_ws);
	dev->slatecom_current_state = SLATECOM_STATE_UNKNOWN;
	rc = slatecom_rpmsg_init(dev);
	if (rc)
		return -ENODEV;
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
	.write          = slatecom_char_write,
	.release        = slatecom_char_close,
	.unlocked_ioctl = slate_com_ioctl,
};

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
		strscpy(dspss_state, "error", BUF_SIZE);
		srcu_notifier_call_chain(&slatecom_notifier_chain,
					 DSP_ERROR, NULL);
	} else {
		statee.e_type = SLATE_DSP_READY;
		strscpy(dspss_state, "ready", BUF_SIZE);
		srcu_notifier_call_chain(&slatecom_notifier_chain,
					 DSP_READY, NULL);
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
		strscpy(btss_state, "error", BUF_SIZE);
		srcu_notifier_call_chain(&slatecom_notifier_chain,
					 BT_ERROR, NULL);
	} else {
		statee.e_type = SLATE_BT_READY;
		strscpy(btss_state, "ready", BUF_SIZE);
		srcu_notifier_call_chain(&slatecom_notifier_chain,
					 BT_READY, NULL);
	}
	send_uevent(&statee);
}
EXPORT_SYMBOL(set_slate_bt_state);

void *slatecom_register_notifier(struct notifier_block *nb)
{
	int ret = 0;

	ret = srcu_notifier_chain_register(&slatecom_notifier_chain, nb);
	if (ret < 0)
		return ERR_PTR(ret);

	return &slatecom_notifier_chain;
}
EXPORT_SYMBOL(slatecom_register_notifier);

int slatecom_unregister_notifier(void *notify, struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(notify, nb);
}
EXPORT_SYMBOL(slatecom_unregister_notifier);

static int __init init_slate_com_dev(void)
{
	int ret, i = 0;

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

	srcu_init_notifier_head(&slatecom_notifier_chain);

	return 0;
}

static void __exit exit_slate_com_dev(void)
{
	int i = 0;

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
