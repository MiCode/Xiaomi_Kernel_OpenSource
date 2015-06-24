/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <video/msm_dba.h>
#include <linux/switch.h>

#include "mdss_dba_utils.h"
#include "mdss_hdmi_edid.h"
#include "mdss_fb.h"

struct mdss_dba_utils_data {
	struct msm_dba_ops ops;
	bool hpd_state;
	bool audio_switch_registered;
	bool display_switch_registered;
	struct switch_dev sdev_display;
	struct switch_dev sdev_audio;
	struct kobject *kobj;
	struct mdss_panel_info *pinfo;
	void *dba_data;
	void *edid_data;
	u8 *edid_buf;
	u32 edid_buf_size;
};

static struct mdss_dba_utils_data *mdss_dba_utils_get_data(
	struct device *device)
{
	struct msm_fb_data_type *mfd;
	struct mdss_panel_info *pinfo;
	struct fb_info *fbi;
	struct mdss_dba_utils_data *udata = NULL;

	if (!device) {
		pr_err("Invalid device data\n");
		goto end;
	}

	fbi = dev_get_drvdata(device);
	if (!fbi) {
		pr_err("Invalid fbi data\n");
		goto end;
	}

	mfd = (struct msm_fb_data_type *)fbi->par;
	if (!mfd) {
		pr_err("Invalid mfd data\n");
		goto end;
	}

	pinfo = mfd->panel_info;
	if (!pinfo) {
		pr_err("Invalid pinfo data\n");
		goto end;
	}

	udata = pinfo->dba_data;
end:
	return udata;
}

static void mdss_dba_utils_send_display_notification(
	struct mdss_dba_utils_data *udata, int val)
{
	int state = 0;

	if (!udata) {
		pr_err("invalid input\n");
		return;
	}

	if (!udata->display_switch_registered) {
		pr_err("display switch not registered\n");
		return;
	}

	state = udata->sdev_display.state;

	switch_set_state(&udata->sdev_display, val);

	pr_debug("cable state %s %d\n",
		udata->sdev_display.state == state ?
		"is same" : "switched to",
		udata->sdev_display.state);
}

static void mdss_dba_utils_send_audio_notification(
	struct mdss_dba_utils_data *udata, int val)
{
	int state = 0;

	if (!udata) {
		pr_err("invalid input\n");
		return;
	}

	if (!udata->audio_switch_registered) {
		pr_err("audio switch not registered\n");
		return;
	}

	state = udata->sdev_audio.state;

	switch_set_state(&udata->sdev_audio, val);

	pr_debug("audio state %s %d\n",
		udata->sdev_audio.state == state ?
		"is same" : "switched to",
		udata->sdev_audio.state);
}

static ssize_t mdss_dba_utils_sysfs_rda_connected(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct mdss_dba_utils_data *udata = NULL;

	if (!dev) {
		pr_err("invalid device\n");
		return -EINVAL;
	}

	udata = mdss_dba_utils_get_data(dev);

	if (!udata) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "%d\n", udata->hpd_state);
	pr_debug("'%d'\n", udata->hpd_state);

	return ret;
}

static DEVICE_ATTR(connected, S_IRUGO,
		mdss_dba_utils_sysfs_rda_connected, NULL);

static struct attribute *mdss_dba_utils_fs_attrs[] = {
	&dev_attr_connected.attr,
	NULL,
};

static struct attribute_group mdss_dba_utils_fs_attrs_group = {
	.attrs = mdss_dba_utils_fs_attrs,
};

static int mdss_dba_utils_sysfs_create(struct kobject *kobj)
{
	int rc;

	if (!kobj) {
		pr_err("invalid input\n");
		return -ENODEV;
	}

	rc = sysfs_create_group(kobj, &mdss_dba_utils_fs_attrs_group);
	if (rc) {
		pr_err("failed, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static void mdss_dba_utils_sysfs_remove(struct kobject *kobj)
{
	if (!kobj) {
		pr_err("invalid input\n");
		return;
	}

	sysfs_remove_group(kobj, &mdss_dba_utils_fs_attrs_group);
}

static void mdss_dba_utils_dba_cb(void *data, enum msm_dba_callback_event event)
{
	int ret = -EINVAL;
	struct mdss_dba_utils_data *udata = data;
	bool pluggable = false;

	if (!udata) {
		pr_err("Invalid data\n");
		return;
	}

	pr_debug("event: %d\n", event);

	if (udata->pinfo)
		pluggable = udata->pinfo->is_pluggable;

	switch (event) {
	case MSM_DBA_CB_HPD_CONNECT:
		if (udata->ops.get_raw_edid) {
			ret = udata->ops.get_raw_edid(udata->dba_data,
				udata->edid_buf_size, udata->edid_buf, 0);

			if (!ret)
				hdmi_edid_parser(udata->edid_data);
			else
				pr_err("failed to get edid%d\n", ret);
		}

		if (pluggable) {
			mdss_dba_utils_send_display_notification(udata, 1);
			mdss_dba_utils_send_audio_notification(udata, 1);
		} else {
			mdss_dba_utils_video_on(udata, udata->pinfo);
		}

		udata->hpd_state = true;
		break;

	case MSM_DBA_CB_HPD_DISCONNECT:
		if (pluggable) {
			mdss_dba_utils_send_audio_notification(udata, 0);
			mdss_dba_utils_send_display_notification(udata, 0);
		} else {
			mdss_dba_utils_video_off(udata);
		}

		udata->hpd_state = false;
		break;

	default:
		break;
	}
}

/**
 * mdss_dba_utils_video_on() - Allow clients to switch on the video
 * @data: DBA utils instance which was allocated during registration
 * @pinfo: detailed panel information like x, y, porch values etc
 *
 * This API is used to power on the video on device registered
 * with DBA.
 *
 * Return: returns the result of the video on call on device.
 */
int mdss_dba_utils_video_on(void *data, struct mdss_panel_info *pinfo)
{
	struct mdss_dba_utils_data *ud = data;
	struct msm_dba_video_cfg video_cfg;
	int ret = -EINVAL;

	if (!ud || !pinfo) {
		pr_err("invalid input\n");
		goto end;
	}

	memset(&video_cfg, 0, sizeof(video_cfg));

	video_cfg.h_active = pinfo->xres;
	video_cfg.v_active = pinfo->yres;
	video_cfg.h_front_porch = pinfo->lcdc.h_front_porch;
	video_cfg.v_front_porch = pinfo->lcdc.v_front_porch;
	video_cfg.h_back_porch = pinfo->lcdc.h_back_porch;
	video_cfg.v_back_porch = pinfo->lcdc.v_back_porch;
	video_cfg.h_pulse_width = pinfo->lcdc.h_pulse_width;
	video_cfg.v_pulse_width = pinfo->lcdc.v_pulse_width;
	video_cfg.pclk_khz = pinfo->clk_rate / 1000;

	if (ud->ops.video_on)
		ret = ud->ops.video_on(ud->dba_data, true, &video_cfg, 0);

	if (ud->ops.hdcp_enable)
		ret = ud->ops.hdcp_enable(ud->dba_data, true, true, 0);
end:
	return ret;
}

/**
 * mdss_dba_utils_video_off() - Allow clients to switch off the video
 * @data: DBA utils instance which was allocated during registration
 *
 * This API is used to power off the video on device registered
 * with DBA.
 *
 * Return: returns the result of the video off call on device.
 */
int mdss_dba_utils_video_off(void *data)
{
	struct mdss_dba_utils_data *ud = data;
	int ret = -EINVAL;

	if (!ud) {
		pr_err("invalid input\n");
		goto end;
	}

	if (ud->ops.video_on)
		ret = ud->ops.video_on(ud->dba_data, false, NULL, 0);

	if (ud->ops.hdcp_enable)
		ret = ud->ops.hdcp_enable(ud->dba_data, false, false, 0);
end:
	return ret;
}

/**
 * mdss_dba_utils_init() - Allow clients to register with DBA utils
 * @uid: Initialization data for registration.
 *
 * This API lets the client to register with DBA Utils module.
 * This allocate utils' instance and register with DBA (Display
 * Bridge Abstract). Creates sysfs nodes and switch nodes to interact
 * with other modules. Also registers with EDID parser to parse
 * the EDID buffer.
 *
 * Return: Instance of DBA utils which needs to be sent as parameter
 * when calling DBA utils APIs.
 */
void *mdss_dba_utils_init(struct mdss_dba_utils_init_data *uid)
{
	struct hdmi_edid_init_data edid_init_data;
	struct mdss_dba_utils_data *udata = NULL;
	struct msm_dba_reg_info info;
	int ret = 0;

	if (!uid) {
		pr_err("invalid input\n");
		ret = -EINVAL;
		goto error;
	}

	udata = kzalloc(sizeof(*udata), GFP_KERNEL);
	if (!udata) {
		pr_err("Not enough Memory\n");
		ret = -ENOMEM;
		goto error;
	}

	memset(&edid_init_data, 0, sizeof(edid_init_data));
	memset(&info, 0, sizeof(info));

	/* initialize DBA registration data */
	strlcpy(info.client_name, uid->client_name, MSM_DBA_CLIENT_NAME_LEN);
	strlcpy(info.chip_name, uid->chip_name, MSM_DBA_CHIP_NAME_MAX_LEN);
	info.instance_id = uid->instance_id;
	info.cb = mdss_dba_utils_dba_cb;
	info.cb_data = udata;

	/* register client with DBA and get device's ops*/
	if (IS_ENABLED(CONFIG_MSM_DBA)) {
		udata->dba_data = msm_dba_register_client(&info, &udata->ops);
		if (IS_ERR_OR_NULL(udata->dba_data)) {
			pr_err("ds not configured\n");
			ret = PTR_ERR(udata->dba_data);
			goto error;
		}
	} else {
		pr_err("DBA not enabled\n");
		ret = -ENODEV;
		goto error;
	}

	/* create sysfs nodes for other modules to intract with utils */
	ret = mdss_dba_utils_sysfs_create(uid->kobj);
	if (ret) {
		pr_err("sysfs creation failed\n");
		goto error;
	}

	/* keep init data for future use */
	udata->kobj = uid->kobj;
	udata->pinfo = uid->pinfo;

	/* create switch device to update display modules */
	udata->sdev_display.name = "bridge_display";
	ret = switch_dev_register(&udata->sdev_display);
	if (ret) {
		pr_err("display switch registration failed\n");
		goto error;
	}
	udata->display_switch_registered = true;

	/* create switch device to update audio modules */
	udata->sdev_audio.name = "hdmi_audio";
	ret = switch_dev_register(&udata->sdev_audio);
	if (ret) {
		pr_err("audio switch registration failed\n");
		goto error;
	}

	udata->audio_switch_registered = true;

	/* Initialize EDID feature */
	edid_init_data.kobj = uid->kobj;

	/* register with edid module for parsing edid buffer */
	udata->edid_data = hdmi_edid_init(&edid_init_data);
	if (!udata->edid_data) {
		pr_err("edid parser init failed\n");
		ret = -ENODEV;
		goto error;
	}

	/* update edid data to retrieve it back in edid parser */
	if (uid->pinfo)
		uid->pinfo->edid_data = udata->edid_data;

	/* get edid buffer from edid parser */
	udata->edid_buf = edid_init_data.buf;
	udata->edid_buf_size = edid_init_data.buf_size;

	/* power on downstream device */
	if (udata->ops.power_on) {
		ret = udata->ops.power_on(udata->dba_data, true, 0);
		if (ret)
			goto error;
	}

	return udata;

error:
	mdss_dba_utils_deinit(udata);
	return ERR_PTR(ret);
}

/**
 * mdss_dba_utils_deinit() - Allow clients to de-register with DBA utils
 * @data: DBA utils data that was allocated during registration.
 *
 * This API will release all the resources allocated during registration
 * and delete the DBA utils instance.
 */
void mdss_dba_utils_deinit(void *data)
{
	struct mdss_dba_utils_data *udata = data;

	if (!udata) {
		pr_err("invalid input\n");
		return;
	}

	if (udata->edid_data)
		hdmi_edid_deinit(udata->edid_data);

	if (udata->pinfo)
		udata->pinfo->edid_data = NULL;

	if (udata->audio_switch_registered)
		switch_dev_unregister(&udata->sdev_audio);

	if (udata->display_switch_registered)
		switch_dev_unregister(&udata->sdev_display);

	if (udata->kobj)
		mdss_dba_utils_sysfs_remove(udata->kobj);

	if (IS_ENABLED(CONFIG_MSM_DBA)) {
		if (!IS_ERR_OR_NULL(udata->dba_data))
			msm_dba_deregister_client(udata->dba_data);
	}

	kfree(udata);
}
