// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#define pr_fmt(msg) "slatecom_dev:" msg

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
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
#include <linux/compat.h>
#include <linux/qseecom_kernel.h>
#include <linux/soc/qcom/slatecom_interface.h>
#include <linux/soc/qcom/slate_events_bridge_intf.h>

#include <uapi/linux/slatecom_interface.h>

#include "slatecom.h"
#include "slatecom_rpmsg.h"


#define SLATECOM "slate_com_dev"
#define SLATEDAEMON_LDO09_LPM_VTG 0
#define SLATEDAEMON_LDO09_NPM_VTG 10000
#define SLATEDAEMON_LDO03_LPM_VTG 0
#define SLATEDAEMON_LDO03_NPM_VTG 10000
#define MPPS_DOWN_EVENT_TO_SLATE_TIMEOUT 3000
#define ADSP_DOWN_EVENT_TO_SLATE_TIMEOUT 3000
#define MAX_APP_NAME_SIZE 100
#define COMPAT_PTR(val) ((void *)((uint64_t)val & 0xffffffffUL))
#define __QAPI_VERSION_MAJOR_SHIFT (24)
#define __QAPI_VERSION_MINOR_SHIFT (16)
#define __QAPI_VERSION_NIT_SHIFT (0)
#define __QAPI_VERSION_MAJOR_MASK (0xff000000)
#define __QAPI_VERSION_MINOR_MASK (0x00ff0000)
#define __QAPI_VERSION_NIT_MASK (0x0000ffff)
#define SCOM_GLINK_INTENT_SIZE 308

/*pil_slate_intf.h*/
#define RESULT_SUCCESS 0
#define RESULT_FAILURE -1

#define SLATECOM_INTF_N_FILES 2
#define BUF_SIZE 10
#define SECURE_APP		"slateapp"

static char btss_state[BUF_SIZE] = "offline";
static char dspss_state[BUF_SIZE] = "offline";
static void ssr_register(void);
static int setup_pmic_gpio15(void);
static unsigned int pmic_gpio15 = -1;
static int slate_boot_status;
struct rproc *slatecom_rproc;

/* tzapp command list.*/
enum slate_tz_commands {
	SLATE_RPROC_RAMDUMP,
	SLATE_RPROC_IMAGE_LOAD,
	SLATE_RPROC_AUTH_MDT,
	SLATE_RPROC_DLOAD_CONT,
	SLATE_RPROC_GET_SLATE_VERSION,
	SLATE_RPROC_SHUTDOWN,
	SLATE_RPROC_DUMPINFO,
	SLATE_RPROC_UP_INFO,
	SLATE_RPROC_RESET,
};

/* tzapp slate request.*/
struct tzapp_slate_req {
	uint64_t address_fw;
	uint32_t size_fw;
	uint8_t tzapp_slate_cmd;
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

struct rf_power_clk_data {
	struct clk *clk;  /* clock regulator handle */
	const char *name; /* clock name */
	bool is_enabled;  /* is this clock enabled? */
};

struct slatedaemon_priv {
	void *pil_h;
	int app_status;
	unsigned long attrs;
	u32 cmd_status;
	struct device *platform_dev;
	bool slatecom_rpmsg;
	bool slate_resp_cmplt;
	bool slate_unload;
	void *lhndl;
	wait_queue_head_t link_state_wait;
	char rx_buf[SCOM_GLINK_INTENT_SIZE];
	struct work_struct slatecom_up_work;
	struct work_struct slatecom_down_work;
	struct mutex glink_mutex;
	struct mutex slatecom_state_mutex;
	struct mutex cmdsync_lock;
	enum slatecom_state slatecom_current_state;
	struct workqueue_struct *slatecom_wq;
	struct wakeup_source slatecom_ws;
	struct qseecom_handle *qseecom_handle;
	struct rf_power_clk_data *rf_clk_2; /* rf clock */
};

static void *slatecom_intf_drv;

struct slate_event {
	enum slate_event_type e_type;
};

struct service_info {
	const char                      name[32];
	const char                      ssr_domains[32];
	int                             domain_id;
	void                            *handle;
	struct notifier_block           *nb;
};

static struct slatedaemon_priv *dev;
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
static struct platform_device *slate_pdev;
struct kobject *kobj_ref;

static ssize_t slate_bt_state_sysfs_read
			(struct kobject *class, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, BUF_SIZE, btss_state);
}

static ssize_t slate_dsp_state_sysfs_read
			(struct kobject *class, struct kobj_attribute *attr, char *buf)
{
	return	scnprintf(buf, BUF_SIZE, dspss_state);
}

struct kobj_attribute slatecom_attr[] = {
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
	wake_up(&dev->link_state_wait);
}

void slatecom_rx_msg(void *data, int len)
{
	struct slatedaemon_priv *dev =
		container_of(slatecom_intf_drv, struct slatedaemon_priv, lhndl);

	if (len > SCOM_GLINK_INTENT_SIZE) {
		pr_err("Invalid slatecom_intf glink intent size\n");
		return;
	}
	dev->slate_resp_cmplt = true;
	wake_up(&dev->link_state_wait);
	memcpy(dev->rx_buf, data, len);
}

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
static int send_state_change_cmd(uint64_t state)
{
	int ret = 0;
	struct msg_header_t msg_header = {0, 0};
	struct slatedaemon_priv *dev = container_of(slatecom_intf_drv,
					struct slatedaemon_priv,
					lhndl);

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
		pr_err("MSM State transtion event:%llu cmd failed\n", state);
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
	case SLATECOM_REG_READ:
		ret = slatecom_reg_read(handle, fui_obj_msg->cmd,
				fui_obj_msg->num_of_words,
				read_buf);
		break;
	case SLATECOM_AHB_READ:
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
	if (ret < 0)
		pr_err("%s failed\n", __func__);
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
	case SLATECOM_REG_WRITE:
		ret = slatecom_reg_write(handle, fui_obj_msg->cmd,
				fui_obj_msg->num_of_words,
				write_buf);
		break;
	case SLATECOM_AHB_WRITE:
		ret = slatecom_ahb_write(handle,
				fui_obj_msg->slate_address,
				fui_obj_msg->num_of_words,
				write_buf);
		break;
	}
	if (ret < 0)
		pr_err("%s failed\n", __func__);
	kfree(write_buf);
	return ret;
}

static int slatecom_get_rproc_handle(struct slatedaemon_priv *priv)
{
	struct platform_device *pdev = NULL;
	int ret;
	const char *firmware_name = NULL;
	phandle rproc_phandle;

	pdev = slate_pdev;

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
		slatecom_rproc = (struct rproc *)priv->pil_h;
		return 0;
	}

fail:
	pr_err("%s: SLATE get handle failed\n", __func__);
	return -EFAULT;
}

static int dt_parse_clk_info(struct device *dev,
		struct rf_power_clk_data **clk_data)
{
	int ret = -EINVAL;
	struct rf_power_clk_data *clk = NULL;
	struct device_node *np = dev->of_node;

	*clk_data = NULL;
	if (of_parse_phandle(np, "clocks", 0)) {
		clk = devm_kzalloc(dev, sizeof(*clk), GFP_KERNEL);
		if (!clk) {
			ret = -ENOMEM;
			goto err;
		}

		/* Allocated 20 bytes size buffer for clock name string */
		clk->name = devm_kzalloc(dev, 20, GFP_KERNEL);

		/* Parse clock name from node */
		ret = of_property_read_string_index(np, "clock-names", 0,
				&(clk->name));
		if (ret < 0) {
			pr_err("%s: reading \"clock-names\" failed\n",
				__func__);
			return ret;
		}

		clk->clk = devm_clk_get(dev, clk->name);
		if (IS_ERR(clk->clk)) {
			ret = PTR_ERR(clk->clk);
			pr_err("%s: failed to get %s, ret (%d)\n",
				__func__, clk->name, ret);
			clk->clk = NULL;
			return ret;
		}
		*clk_data = clk;
	} else {
		pr_err("%s: clocks is not provided in device tree\n", __func__);
	}

err:
	return ret;
}

static int rf_clk_enable(struct rf_power_clk_data *clk)
{
	int rc = 0;

	pr_debug("%s: %s\n", __func__, clk->name);

	/* Get the clock handle for vreg */
	if (!clk->clk || clk->is_enabled) {
		pr_err("%s: error - node: %p, clk->is_enabled:%d\n",
			__func__, clk->clk, clk->is_enabled);
		return -EINVAL;
	}

	rc = clk_prepare_enable(clk->clk);
	if (rc) {
		pr_err("%s: failed to enable %s, rc(%d)\n",
				__func__, clk->name, rc);
		return rc;
	}

	clk->is_enabled = true;
	return rc;
}

static int rf_clk_disable(struct rf_power_clk_data *clk)
{
	int rc = 0;

	pr_debug("%s: %s\n", __func__, clk->name);

	/* Get the clock handle for vreg */
	if (!clk->clk || !clk->is_enabled) {
		pr_err("%s: error - node: %p, clk->is_enabled:%d\n",
			__func__, clk->clk, clk->is_enabled);
		return -EINVAL;
	}
	clk_disable_unprepare(clk->clk);

	clk->is_enabled = false;
	return rc;
}

static int slatecom_fw_load(struct slatedaemon_priv *priv)
{
	int ret = 0;

	if (dev->rf_clk_2)
		rf_clk_enable(dev->rf_clk_2);

	if (!priv->pil_h) {
		pr_err("%s: Getting rproc handle\n", __func__);
		ret = slatecom_get_rproc_handle(priv);
	}
	if (ret == 0) {
		ret = rproc_boot(priv->pil_h);
		if (ret) {
			pr_err("%s: rproc boot failed, err: %d\n",
				__func__, ret);
			priv->pil_h = NULL;
			slate_boot_status = 0;
			goto fail;
		}
		slate_boot_status = 1;
		dev->slate_unload = false;
		pr_info("%s: SLATE image is loaded\n", __func__);
		return 0;
	}
fail:
	pr_err("%s: SLATE image loading failed\n", __func__);
	return -EFAULT;
}

static void slatecom_fw_unload(struct slatedaemon_priv *priv)
{
	if (!priv) {
		pr_err("%s: handle not found\n", __func__);
		return;
	}
	if (priv->pil_h) {
		pr_err("%s: calling subsystem put\n", __func__);
		priv->slate_unload = true;
		rproc_shutdown(priv->pil_h);
		priv->slate_unload = false;
		priv->pil_h = NULL;
		slate_boot_status = 0;
	}
}

/**
 * send_slate_boot_status send state boot status event to clients
 *
 */
static int send_slate_boot_status(enum boot_status event)
{
	int rc;
	char *event_buf;
	unsigned int event_buf_size;

	if (event == SLATE_UPDATE_START)
		set_slate_bt_state(false);
	else if (event == SLATE_UPDATE_DONE)
		set_slate_bt_state(true);

	event_buf_size = sizeof(enum boot_status);

	event_buf = kmemdup((char *)&event, event_buf_size, GFP_KERNEL);
	if (!event_buf)
		return -ENOMEM;

	rc = seb_send_event(SLATE_STATUS, event_buf,
					event_buf_size);
	if (rc < 0) {
		pr_err("Failed to send SLATE_STATUS event, rc=%d\n",
			rc);
		goto free_event_buf;
	}

	pr_info("Send SLATE_STATUS event successful\n");

free_event_buf:
	kfree(event_buf);

	return rc;
}

/**
 * load_slate_tzapp() - Called to load TZ app.
 * @pbd: struct containing private SLATE data.
 *
 * Return: 0 on success. Error code on failure.
 */

static int load_slate_tzapp(struct slatedaemon_priv *pbd)
{
	int rc;

	/* return success if already loaded */
	if (pbd->qseecom_handle && !pbd->app_status)
		return 0;
	/* Load the APP */
	rc = qseecom_start_app(&pbd->qseecom_handle, SECURE_APP, SZ_4K);
	if (rc < 0) {
		dev_err(pbd->platform_dev, "SLATE TZ app load failure\n");
		pbd->app_status = RESULT_FAILURE;
		return -EIO;
	}
	pbd->app_status = RESULT_SUCCESS;
	return 0;
}

/**
 * get_cmd_rsp_buffers() - Function sets cmd & rsp buffer pointers and
 *                         aligns buffer lengths
 * @hdl:	index of qseecom_handle
 * @cmd:	req buffer - set to qseecom_handle.sbuf
 * @cmd_len:	ptr to req buffer len
 * @rsp:	rsp buffer - set to qseecom_handle.sbuf + offset
 * @rsp_len:	ptr to rsp buffer len
 *
 * Return: Success always .
 */
static int get_cmd_rsp_buffers(struct qseecom_handle *handle, void **cmd,
			uint32_t *cmd_len, void **rsp, uint32_t *rsp_len)
{
	*cmd = handle->sbuf;
	if (*cmd_len & QSEECOM_ALIGN_MASK)
		*cmd_len = QSEECOM_ALIGN(*cmd_len);
	*rsp = handle->sbuf + *cmd_len;
	if (*rsp_len & QSEECOM_ALIGN_MASK)
		*rsp_len = QSEECOM_ALIGN(*rsp_len);
	return 0;
}

/**
 * slate_tzapp_comm() - Function called to communicate with TZ APP.
 * @pbd: struct containing private SLATE data.
 * @req: struct containing command and parameters.
 *
 * Return: 0 on success. Error code on failure.
 */

static long slate_tzapp_comm(struct slatedaemon_priv *pbd,
				struct tzapp_slate_req *req)
{
	struct tzapp_slate_req *slate_tz_req;
	struct tzapp_slate_rsp *slate_tz_rsp;
	int rc, req_len, rsp_len;

	/* Fill command structure */
	req_len = sizeof(struct tzapp_slate_req);
	rsp_len = sizeof(struct tzapp_slate_rsp);

	mutex_lock(&pbd->cmdsync_lock);
	rc = get_cmd_rsp_buffers(pbd->qseecom_handle,
		(void **)&slate_tz_req, &req_len,
		(void **)&slate_tz_rsp, &rsp_len);
	if (rc)
		goto end;

	slate_tz_req->tzapp_slate_cmd = req->tzapp_slate_cmd;
	slate_tz_req->address_fw = req->address_fw;
	slate_tz_req->size_fw = req->size_fw;
	rc = qseecom_send_command(pbd->qseecom_handle,
		(void *)slate_tz_req, req_len, (void *)slate_tz_rsp, rsp_len);

	mutex_unlock(&pbd->cmdsync_lock);
	pr_debug("SLATE PIL qseecom returned with value 0x%x and status 0x%x\n",
		rc, slate_tz_rsp->status);
	if (rc || slate_tz_rsp->status)
		pbd->cmd_status = slate_tz_rsp->status;
	else
		pbd->cmd_status = 0;
	return rc;
end:
	mutex_unlock(&pbd->cmdsync_lock);
	return rc;
}


int slate_soft_reset(void)
{
	struct tzapp_slate_req slate_tz_req;
	struct slatedaemon_priv *tzapp = container_of(slatecom_intf_drv,
					struct slatedaemon_priv,
					lhndl);
	int ret;

	ret = load_slate_tzapp(tzapp);
	if (ret) {
		dev_err(&slate_pdev->dev, "%s: SLATE TZ app load failure\n", __func__);
			return ret;
	}
	slate_tz_req.tzapp_slate_cmd = SLATE_RPROC_RESET;
	slate_tz_req.address_fw = 0;
	slate_tz_req.size_fw = 0;
	ret = slate_tzapp_comm(tzapp, &slate_tz_req);
	if (ret || tzapp->cmd_status)
		dev_err(&slate_pdev->dev,
			"%s: Failed to send reset signal to tzapp\n",
			__func__);
	return ret;
}
EXPORT_SYMBOL_GPL(slate_soft_reset);

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
EXPORT_SYMBOL_GPL(send_wlan_state);

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
	size_t len;

	len = tui_obj_msg->num_of_words * sizeof(uint32_t);
	write_buf = memdup_user((COMPAT_PTR(tui_obj_msg->buffer)), len);
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
	kfree(write_buf);
return ret;
}

static int send_debug_config(uint64_t config)
{
	int ret = 0;
	struct msg_header_t msg_header = {0, 0};
	struct slatedaemon_priv *dev = container_of(slatecom_intf_drv,
					struct slatedaemon_priv,
					lhndl);

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
		pr_err("Invalid debug config cmd:%llu\n", config);
		return -EINVAL;
	}
	ret = slatecom_tx_msg(dev, &msg_header.opcode, sizeof(msg_header.opcode));

	if (ret < 0)
		pr_err("failed to send debug config cmd:%llu\n", config);
	return ret;
}

int set_slate_boot_mode(uint32_t mode)
{
	gpio_direction_output(pmic_gpio15, 0);
	gpio_set_value(pmic_gpio15, mode);
	/*get back pmic gpio*/
	if (gpio_get_value(pmic_gpio15) == mode)
		return RESULT_SUCCESS;
	return RESULT_FAILURE;
}
EXPORT_SYMBOL_GPL(set_slate_boot_mode);

int get_slate_boot_mode(void)
{
	int mode = -1;

	if (gpio_direction_input(pmic_gpio15) < 0)
		return RESULT_FAILURE;
	mode = gpio_get_value(pmic_gpio15);

	return mode;
}
EXPORT_SYMBOL_GPL(get_slate_boot_mode);

bool is_slate_unload_only(void)
{
	struct slatedaemon_priv *dev =
		container_of(slatecom_intf_drv, struct slatedaemon_priv, lhndl);

	return dev->slate_unload;
}
EXPORT_SYMBOL_GPL(is_slate_unload_only);

static int send_get_fw_version(struct slate_ui_data *ui_obj_msg)
{
	int ret = 0;
	struct msg_header_t msg_header = {0, 0};
	struct wear_firmware_info *fw_info = NULL;
	void __user *read_buf = COMPAT_PTR(ui_obj_msg->buffer);
	struct slatedaemon_priv *dev = container_of(slatecom_intf_drv,
					struct slatedaemon_priv,
					lhndl);

	msg_header.opcode = GMI_WEAR_MGR_GET_FIRMWARE_DETAILS;
	ret = slatecom_tx_msg(dev, &msg_header.opcode, sizeof(msg_header.opcode));
	fw_info = (struct wear_firmware_info *)dev->rx_buf;
	if (!ret && copy_to_user(read_buf, fw_info, sizeof(struct wear_firmware_info))) {
		pr_err("copy to user failed\n");
		ret = -EFAULT;
	}
	return ret;
}
static int send_ipc_cmd_to_slate(struct slate_ui_data *ui_obj_msg)
{
	int ret = 0;
	uint32_t cmd = ui_obj_msg->cmd;

	switch (cmd) {
	case STATE_TRANSITION:
		ret = send_state_change_cmd(ui_obj_msg->write);
		break;
	case TIME_SYNC:
		ret = send_time_sync(ui_obj_msg);
		break;
	case DEBUG_CONFIG:
		ret = send_debug_config(ui_obj_msg->write);
		break;
	case GET_VERSION:
		ret = send_get_fw_version(ui_obj_msg);
		break;
	default:
		pr_err("Invalid ipc cmd:%d\n", cmd);
		return -EINVAL;
	}
	return ret;
}

static int send_boot_cmd_to_slate(struct slate_ui_data *ui_obj_msg)
{
	int ret = 0;
	uint32_t cmd = ui_obj_msg->cmd;

	switch (cmd) {
	case SOFT_RESET:
		slate_soft_reset();
		break;
	case TWM_EXIT:
		twm_exit = true;
		dev->slate_unload = true;
		break;
	case AON_APP_RUNNING:
		slate_app_running = true;
		break;
	case LOAD:
		if (!slate_boot_status)
			ret = slatecom_fw_load(dev);
		else {
			pr_info("slate is already loaded\n");
			dev->slate_unload = false;
			ret = -EFAULT;
		}
		break;
	case UNLOAD:
		slatecom_fw_unload(dev);
		break;
	case GET_BOOT_MODE:
		ret = get_slate_boot_mode();
		break;
	case SET_BOOT_MODE:
		ret = set_slate_boot_mode(ui_obj_msg->write);
		break;
	case CMD_SAVE_AON_DUMP:
		if (!dev->pil_h)
			ret = slatecom_get_rproc_handle(dev);
		if (ret == 0)
			slatecom_rproc->ops->coredump(slatecom_rproc);
		else
			pr_err("failed to get rproc, skip coredump collection\n");
		break;
	case BOOT_STATUS:
		ret = send_slate_boot_status(ui_obj_msg->write);
		break;
	default:
		pr_err("Invalid boot cmd:%d\n", cmd);
		return -EINVAL;
	}
	return ret;
}

static long slate_com_ioctl(struct file *filp,
		unsigned int ui_slatecom_cmd, unsigned long arg)
{
	int ret = 0;
	struct slate_ui_data ui_obj_msg = {0, 0, 0, 0, 0, NULL};

	if (!dev || (dev->slatecom_current_state == SLATECOM_STATE_UNKNOWN) || (filp == NULL)) {
		pr_err("slatecom driver not initialized\n");
		return -EINVAL;
	}

	if (arg != 0) {
		if (copy_from_user(&ui_obj_msg, (void __user *) arg,
				sizeof(ui_obj_msg))) {
			pr_err("The copy from user failed\n");
			ret = -EFAULT;
		}
	}
	switch (ui_slatecom_cmd) {
	case SLATECOM_REG_READ:
	case SLATECOM_AHB_READ:
		ret = slatechar_read_cmd(&ui_obj_msg,
				ui_slatecom_cmd);
		break;
	case SLATECOM_AHB_WRITE:
	case SLATECOM_REG_WRITE:
		ret = slatechar_write_cmd(&ui_obj_msg, ui_slatecom_cmd);
		break;
	case SLATECOM_SET_SPI_FREE:
		ret = slatecom_set_spi_state(SLATECOM_SPI_FREE);
		break;
	case SLATECOM_SET_SPI_BUSY:
		ret = slatecom_set_spi_state(SLATECOM_SPI_BUSY);
		break;
	case SLATECOM_MODEM_DOWN2_SLATE:
		ret = modem_down2_slate();
		break;
	case SLATECOM_ADSP_DOWN2_SLATE:
		ret = adsp_down2_slate();
		break;
	case SLATECOM_SEND_IPC_CMD:
		if (dev->slatecom_current_state != SLATECOM_STATE_GLINK_OPEN) {
			pr_err("driver not ready, glink is not open\n");
			return -ENODEV;
		}
		ret = send_ipc_cmd_to_slate(&ui_obj_msg);
		break;
	case SLATECOM_SEND_BOOT_CMD:
		ret = send_boot_cmd_to_slate(&ui_obj_msg);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}

long compat_slate_com_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned int nr = _IOC_NR(cmd);

	return (long)slate_com_ioctl(file, nr, arg);
}

static ssize_t slatecom_char_write(struct file *f, const char __user *buf,
				size_t count, loff_t *off)
{
	unsigned char qcli_cmnd;
	uint32_t opcode;
	int ret = 0;
	struct slatedaemon_priv *dev = NULL;

	if (!slatecom_intf_drv) {
		pr_err("Invalid use-case, slatecom driver is not ready\n");
		return -EINVAL;
	}
	dev = container_of(slatecom_intf_drv,
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
	case 'c':
		if (dev->slatecom_current_state == SLATECOM_STATE_GLINK_OPEN) {
			opcode = GMI_MGR_FORCE_CRASH;
			ret = slatecom_tx_msg(dev, &opcode, sizeof(opcode));
			if (ret < 0)
				pr_err("AON force crash cmd failed\n");
		} else
			pr_debug("glink is not open, skip AON force crash\n");
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
				dev->slatecom_rpmsg, msecs_to_jiffies(TIMEOUT_MS_GLINK_OPEN));
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
	slate_pdev = pdev;
	setup_pmic_gpio15();
	ssr_register();
	dt_parse_clk_info(&pdev->dev,
					&dev->rf_clk_2);
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
	#ifdef CONFIG_COMPAT
	.compat_ioctl   = compat_slate_com_ioctl,
	#endif
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
	struct slatedaemon_priv *dev = container_of(slatecom_intf_drv,
						struct slatedaemon_priv, lhndl);

	switch (opcode) {
	case QCOM_SSR_BEFORE_SHUTDOWN:
		pr_debug("Slate before shutdown\n");
		slatee.e_type = SLATE_BEFORE_POWER_DOWN;
		slatecom_set_spi_state(SLATECOM_SPI_BUSY);
		send_uevent(&slatee);
		queue_work(dev->slatecom_wq, &dev->slatecom_down_work);
		break;
	case QCOM_SSR_AFTER_SHUTDOWN:
		pr_debug("Slate after shutdown\n");
		slatee.e_type = SLATE_AFTER_POWER_DOWN;
		slatecom_slatedown_handler();
		send_uevent(&slatee);
		set_slate_bt_state(false);
		set_slate_dsp_state(false);
		if (dev->rf_clk_2)
			rf_clk_enable(dev->rf_clk_2);
		break;
	case QCOM_SSR_BEFORE_POWERUP:
		pr_debug("Slate before powerup\n");
		slatee.e_type = SLATE_BEFORE_POWER_UP;
		send_uevent(&slatee);
		break;
	case QCOM_SSR_AFTER_POWERUP:
		pr_debug("Slate after powerup\n");
		twm_exit = false;
		slatee.e_type = SLATE_AFTER_POWER_UP;
		slatecom_set_spi_state(SLATECOM_SPI_FREE);
		if (dev->slatecom_current_state == SLATECOM_STATE_INIT ||
			dev->slatecom_current_state == SLATECOM_STATE_SLATE_SSR)
			queue_work(dev->slatecom_wq, &dev->slatecom_up_work);
		send_uevent(&slatee);
		if (dev->rf_clk_2)
			rf_clk_disable(dev->rf_clk_2);
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
	struct msg_header_t msg_header = {0, 0};
	int ret = 0;

	switch (opcode) {
	case QCOM_SSR_BEFORE_SHUTDOWN:
		modeme.e_type = MODEM_BEFORE_POWER_DOWN;
		reinit_completion(&slate_modem_down_wait);
		send_uevent(&modeme);
		msg_header.opcode = GMI_MGR_SSR_MPSS_DOWN_PRE_NOTIFICATION;
		ret = slatecom_tx_msg(dev, &(msg_header.opcode), sizeof(msg_header.opcode));
		if (ret < 0)
			pr_err("failed to send mdsp down pre-event to slate\n");
		break;
	case QCOM_SSR_AFTER_SHUTDOWN:
		modeme.e_type = MODEM_AFTER_POWER_DOWN;
		reinit_completion(&slate_modem_down_wait);
		send_uevent(&modeme);
		msg_header.opcode = GMI_MGR_SSR_MPSS_DOWN_NOTIFICATION;
		ret = slatecom_tx_msg(dev, &(msg_header.opcode), sizeof(msg_header.opcode));
		if (ret < 0)
			pr_err("failed to send mdsp down event to slate\n");
		break;
	case QCOM_SSR_BEFORE_POWERUP:
		modeme.e_type = MODEM_BEFORE_POWER_UP;
		send_uevent(&modeme);
		msg_header.opcode = GMI_MGR_SSR_MPSS_UP_PRE_NOTIFICATION;
		ret = slatecom_tx_msg(dev, &(msg_header.opcode), sizeof(msg_header.opcode));
		if (ret < 0)
			pr_err("failed to send mdsp up pre-event to slate\n");
		break;
	case QCOM_SSR_AFTER_POWERUP:
		modeme.e_type = MODEM_AFTER_POWER_UP;
		send_uevent(&modeme);
		msg_header.opcode = GMI_MGR_SSR_MPSS_UP_NOTIFICATION;
		ret = slatecom_tx_msg(dev, &(msg_header.opcode), sizeof(msg_header.opcode));
		if (ret < 0)
			pr_err("failed to send mdsp up event to slate\n");
		break;
	}
	return NOTIFY_DONE;
}

static int ssr_adsp_cb(struct notifier_block *this,
		unsigned long opcode, void *data)
{
	struct slate_event adspe;
	struct msg_header_t msg_header = {0, 0};
	int ret = 0;

	switch (opcode) {
	case QCOM_SSR_BEFORE_SHUTDOWN:
		adspe.e_type = ADSP_BEFORE_POWER_DOWN;
		reinit_completion(&slate_adsp_down_wait);
		send_uevent(&adspe);
		msg_header.opcode = GMI_MGR_SSR_ADSP_DOWN_PRE_INDICATION;
		ret = slatecom_tx_msg(dev, &(msg_header.opcode), sizeof(msg_header.opcode));
		if (ret < 0)
			pr_err("failed to send adsp down pre-event to slate\n");
		break;
	case QCOM_SSR_AFTER_SHUTDOWN:
		adspe.e_type = ADSP_AFTER_POWER_DOWN;
		reinit_completion(&slate_adsp_down_wait);
		send_uevent(&adspe);
		msg_header.opcode = GMI_MGR_SSR_ADSP_DOWN_INDICATION;
		ret = slatecom_tx_msg(dev, &(msg_header.opcode), sizeof(msg_header.opcode));
		if (ret < 0)
			pr_err("failed to send adsp down event to slate\n");
		break;
	case QCOM_SSR_BEFORE_POWERUP:
		adspe.e_type = ADSP_BEFORE_POWER_UP;
		send_uevent(&adspe);
		msg_header.opcode = GMI_MGR_SSR_ADSP_UP_PRE_INDICATION;
		ret = slatecom_tx_msg(dev, &(msg_header.opcode), sizeof(msg_header.opcode));
		if (ret < 0)
			pr_err("failed to send adsp up pre-event to slate\n");
		break;
	case QCOM_SSR_AFTER_POWERUP:
		adspe.e_type = ADSP_AFTER_POWER_UP;
		send_uevent(&adspe);
		msg_header.opcode = GMI_MGR_SSR_ADSP_UP_INDICATION;
		ret = slatecom_tx_msg(dev, &(msg_header.opcode), sizeof(msg_header.opcode));
		if (ret < 0)
			pr_err("failed to send adsp up event to slate\n");
		break;
	}
	return NOTIFY_DONE;
}

bool is_twm_exit(void)
{
	if (twm_exit)
		return true;
	return false;
}
EXPORT_SYMBOL_GPL(is_twm_exit);

bool is_slate_running(void)
{
	if (slate_app_running) {
		slate_app_running = false;
		return true;
	}
	return false;
}
EXPORT_SYMBOL_GPL(is_slate_running);

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

void *slatecom_register_notifier(struct notifier_block *nb)
{
	int ret = 0;

	ret = srcu_notifier_chain_register(&slatecom_notifier_chain, nb);
	if (ret < 0)
		return ERR_PTR(ret);

	return &slatecom_notifier_chain;
}
EXPORT_SYMBOL_GPL(slatecom_register_notifier);

int slatecom_unregister_notifier(void *notify, struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(notify, nb);
}
EXPORT_SYMBOL_GPL(slatecom_unregister_notifier);

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
		.ssr_domains = "slatefw",
		.domain_id = SSR_DOMAIN_SLATE,
		.nb = &ssr_slate_nb,
		.handle = NULL,
	},
	{
		.name = "SSR_MODEM",
		.ssr_domains = "mpss",
		.domain_id = SSR_DOMAIN_MODEM,
		.nb = &ssr_modem_nb,
		.handle = NULL,
	},
	{
		.name = "SSR_ADSP",
		.ssr_domains = "lpass",
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
					qcom_register_ssr_notifier(
					service_data[i].ssr_domains,
					service_data[i].nb);
			pr_info("subsys registration for id = %d, ssr domain = %s\n",
				service_data[i].domain_id,
				service_data[i].ssr_domains);
			if (IS_ERR_OR_NULL(service_data[i].handle)) {
				pr_err("subsys register failed for id = %d, ssr domain = %s\n",
						service_data[i].domain_id,
						service_data[i].ssr_domains);
				service_data[i].handle = NULL;
			}
		}
	}

}

static int setup_pmic_gpio15(void)
{
	int val = 0;

	val = of_get_named_gpio(slate_pdev->dev.of_node, "qcom,platform-reset-gpio", 0);
	if (val < 0) {
		pr_err("pmic gpio is not found, error=%d\n", val);
		return -EINVAL;
	}
	pmic_gpio15 = val;
	if (gpio_request(pmic_gpio15, "SLATE_EFLASH_STATUS")) {
		dev_err(&slate_pdev->dev,
			"%s Failed to configure SLATE_EFLASH_STATUS gpio\n",
				__func__);
		return -EINVAL;
	}
	return RESULT_SUCCESS;
}

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
	slate_class = class_create(SLATECOM);
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

	for (i = 0; i < SLATECOM_INTF_N_FILES; i++) {
		kobj_ref = kobject_create_and_add(slatecom_attr[i].attr.name, kernel_kobj);
		/*Creating sysfs file for power_state*/
		if (sysfs_create_file(kobj_ref, &slatecom_attr[i].attr)) {
			pr_err("%s: failed to create slate-bt/dsp entry\n", __func__);
			kobject_put(kobj_ref);
			sysfs_remove_file(kernel_kobj, &slatecom_attr[i].attr);
		}
	}

	if (platform_driver_register(&slate_daemon_driver))
		pr_err("%s: failed to register slate-daemon register\n", __func__);

	srcu_init_notifier_head(&slatecom_notifier_chain);
	slatecom_state_init(&set_slate_dsp_state, &set_slate_bt_state);
	slatecom_ctrl_channel_init(&slatecom_intf_notify_glink_channel_state, &slatecom_rx_msg);
	return 0;
}

static void __exit exit_slate_com_dev(void)
{
	int i = 0;

	kobject_put(kobj_ref);
	for (i = 0; i < SLATECOM_INTF_N_FILES; i++)
		sysfs_remove_file(kernel_kobj, &slatecom_attr[i].attr);

	device_destroy(slate_class, slate_dev);
	class_destroy(slate_class);
	cdev_del(&slate_cdev);
	unregister_chrdev_region(slate_dev, 1);
	platform_driver_unregister(&slate_daemon_driver);
	gpio_free(pmic_gpio15);
}

module_init(init_slate_com_dev);
module_exit(exit_slate_com_dev);
MODULE_LICENSE("GPL");
