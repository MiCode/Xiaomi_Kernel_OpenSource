/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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
#include <linux/extcon.h>

#include "mdss_dba_utils.h"
#include "mdss_hdmi_edid.h"
#include "mdss_cec_core.h"
#include "mdss_fb.h"

/* standard cec buf size + 1 byte specific to driver */
#define CEC_BUF_SIZE    (MAX_CEC_FRAME_SIZE + 1)
#define MAX_SWITCH_NAME_SIZE        5
#define MSM_DBA_MAX_PCLK 148500
#define DEFAULT_VIDEO_RESOLUTION HDMI_VFRMT_640x480p60_4_3

struct mdss_dba_utils_data {
	struct msm_dba_ops ops;
	bool hpd_state;
	bool audio_switch_registered;
	bool display_switch_registered;
	struct extcon_dev sdev_display;
	struct extcon_dev sdev_audio;
	struct kobject *kobj;
	struct mdss_panel_info *pinfo;
	void *dba_data;
	void *edid_data;
	void *timing_data;
	void *cec_abst_data;
	u8 *edid_buf;
	u32 edid_buf_size;
	u8 cec_buf[CEC_BUF_SIZE];
	struct cec_ops cops;
	struct cec_cbs ccbs;
	char disp_switch_name[MAX_SWITCH_NAME_SIZE];
	u32 current_vic;
	bool support_audio;
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

static void mdss_dba_utils_notify_display(
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

	extcon_set_state_sync(&udata->sdev_display, 0, val);

	pr_debug("cable state %s %d\n",
		udata->sdev_display.state == state ?
		"is same" : "switched to",
		udata->sdev_display.state);
}

static void mdss_dba_utils_notify_audio(
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

	extcon_set_state_sync(&udata->sdev_audio, 0, val);

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

static ssize_t mdss_dba_utils_sysfs_rda_video_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct mdss_dba_utils_data *udata = NULL;

	if (!dev) {
		pr_debug("invalid device\n");
		return -EINVAL;
	}

	udata = mdss_dba_utils_get_data(dev);

	if (!udata) {
		pr_debug("invalid input\n");
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "%d\n", udata->current_vic);
	pr_debug("'%d'\n", udata->current_vic);

	return ret;
}

static ssize_t mdss_dba_utils_sysfs_wta_hpd(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdss_dba_utils_data *udata = NULL;
	int rc, hpd;

	udata = mdss_dba_utils_get_data(dev);
	if (!udata) {
		pr_debug("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	rc = kstrtoint(buf, 10, &hpd);
	if (rc) {
		pr_debug("%s: kstrtoint failed\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: set value: %d hpd state: %d\n", __func__,
					hpd, udata->hpd_state);
	if (!hpd) {
		if (udata->ops.power_on)
			udata->ops.power_on(udata->dba_data, false, 0);
		return count;
	}

	/* power on downstream device */
	if (udata->ops.power_on)
		udata->ops.power_on(udata->dba_data, true, 0);

	/* check if cable is connected to bridge chip */
	if (udata->ops.check_hpd)
		udata->ops.check_hpd(udata->dba_data, 0);

	return count;
}

static ssize_t mdss_dba_utils_sysfs_rda_hpd(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct mdss_dba_utils_data *udata = NULL;

	if (!dev) {
		pr_debug("invalid device\n");
		return -EINVAL;
	}

	udata = mdss_dba_utils_get_data(dev);

	if (!udata) {
		pr_debug("invalid input\n");
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "%d\n", udata->hpd_state);
	pr_debug("'%d'\n", udata->hpd_state);

	return ret;
}

static DEVICE_ATTR(connected, 0444,
		mdss_dba_utils_sysfs_rda_connected, NULL);

static DEVICE_ATTR(video_mode, 0444,
		mdss_dba_utils_sysfs_rda_video_mode, NULL);

static DEVICE_ATTR(hpd, 0644, mdss_dba_utils_sysfs_rda_hpd,
		mdss_dba_utils_sysfs_wta_hpd);

static struct attribute *mdss_dba_utils_fs_attrs[] = {
	&dev_attr_connected.attr,
	&dev_attr_video_mode.attr,
	&dev_attr_hpd.attr,
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

static bool mdss_dba_check_audio_support(struct mdss_dba_utils_data *udata)
{
	bool dvi_mode = false;
	int audio_blk_size = 0;
	struct msm_hdmi_audio_edid_blk audio_blk;

	if (!udata) {
		pr_debug("%s: Invalid input\n", __func__);
		return false;
	}
	memset(&audio_blk, 0, sizeof(audio_blk));

	/* check if sink is in DVI mode */
	dvi_mode = !hdmi_edid_get_sink_mode(udata->edid_data);

	/* get the audio block size info from EDID */
	hdmi_edid_get_audio_blk(udata->edid_data, &audio_blk);
	audio_blk_size = audio_blk.audio_data_blk_size;

	if (dvi_mode || !audio_blk_size)
		return false;
	else
		return true;
}

static void mdss_dba_utils_dba_cb(void *data, enum msm_dba_callback_event event)
{
	int ret = -EINVAL;
	struct mdss_dba_utils_data *udata = data;
	struct cec_msg msg = {0};
	bool pluggable = false;
	bool operands_present = false;
	u32 no_of_operands, size, i;
	u32 operands_offset = MAX_CEC_FRAME_SIZE - MAX_OPERAND_SIZE;
	struct msm_hdmi_audio_edid_blk blk;

	if (!udata) {
		pr_err("Invalid data\n");
		return;
	}

	pr_debug("event: %d\n", event);

	if (udata->pinfo)
		pluggable = udata->pinfo->is_pluggable;

	switch (event) {
	case MSM_DBA_CB_HPD_CONNECT:
		if (udata->hpd_state)
			break;
		if (udata->ops.get_raw_edid) {
			ret = udata->ops.get_raw_edid(udata->dba_data,
				udata->edid_buf_size, udata->edid_buf, 0);

			if (!ret) {
				hdmi_edid_parser(udata->edid_data);
				/* check whether audio is supported or not */
				udata->support_audio =
					mdss_dba_check_audio_support(udata);
				if (udata->support_audio) {
					hdmi_edid_get_audio_blk(
						udata->edid_data, &blk);
					if (udata->ops.set_audio_block)
						udata->ops.set_audio_block(
							udata->dba_data,
							sizeof(blk), &blk);
				}
			} else {
				pr_err("failed to get edid%d\n", ret);
			}
		}

		if (pluggable) {
			mdss_dba_utils_notify_display(udata, 1);
			if (udata->support_audio)
				mdss_dba_utils_notify_audio(udata, 1);
		} else {
			mdss_dba_utils_video_on(udata, udata->pinfo);
		}

		udata->hpd_state = true;
		break;

	case MSM_DBA_CB_HPD_DISCONNECT:
		if (!udata->hpd_state)
			break;
		if (pluggable) {
			if (udata->support_audio)
				mdss_dba_utils_notify_audio(udata, 0);
			mdss_dba_utils_notify_display(udata, 0);
		} else {
			mdss_dba_utils_video_off(udata);
		}

		udata->hpd_state = false;
		break;

	case MSM_DBA_CB_CEC_READ_PENDING:
		if (udata->ops.hdmi_cec_read) {
			ret = udata->ops.hdmi_cec_read(
				udata->dba_data,
				&size,
				udata->cec_buf, 0);

			if (ret || !size || size > CEC_BUF_SIZE) {
				pr_err("%s: cec read failed\n", __func__);
				return;
			}
		}

		/* prepare cec msg */
		msg.recvr_id   = udata->cec_buf[0] & 0x0F;
		msg.sender_id  = (udata->cec_buf[0] & 0xF0) >> 4;
		msg.opcode     = udata->cec_buf[1];
		msg.frame_size = (udata->cec_buf[MAX_CEC_FRAME_SIZE] & 0x1F);

		operands_present = (msg.frame_size > operands_offset) &&
			(msg.frame_size <= MAX_CEC_FRAME_SIZE);

		if (operands_present) {
			no_of_operands = msg.frame_size - operands_offset;

			for (i = 0; i < no_of_operands; i++)
				msg.operand[i] =
					udata->cec_buf[operands_offset + i];
		}

		ret = udata->ccbs.msg_recv_notify(udata->ccbs.data, &msg);
		if (ret)
			pr_err("%s: failed to notify cec msg\n", __func__);
		break;

	default:
		break;
	}
}

static int mdss_dba_utils_cec_enable(void *data, bool enable)
{
	int ret = -EINVAL;
	struct mdss_dba_utils_data *udata = data;

	if (!udata) {
		pr_err("%s: Invalid data\n", __func__);
		return -EINVAL;
	}

	if (udata->ops.hdmi_cec_on)
		ret = udata->ops.hdmi_cec_on(udata->dba_data, enable, 0);

	return ret;
}

static int mdss_dba_utils_send_cec_msg(void *data, struct cec_msg *msg)
{
	int ret = -EINVAL, i;
	u32 operands_offset = MAX_CEC_FRAME_SIZE - MAX_OPERAND_SIZE;
	struct mdss_dba_utils_data *udata = data;

	u8 buf[MAX_CEC_FRAME_SIZE];

	if (!udata || !msg) {
		pr_err("%s: Invalid data\n", __func__);
		return -EINVAL;
	}

	buf[0] = (msg->sender_id << 4) | msg->recvr_id;
	buf[1] = msg->opcode;

	for (i = 0; i < MAX_OPERAND_SIZE &&
		i < msg->frame_size - operands_offset; i++)
		buf[operands_offset + i] = msg->operand[i];

	if (udata->ops.hdmi_cec_write)
		ret = udata->ops.hdmi_cec_write(udata->dba_data,
			msg->frame_size, (char *)buf, 0);

	return ret;
}

static int mdss_dba_utils_init_switch_dev(struct mdss_dba_utils_data *udata,
	u32 fb_node)
{
	int rc = -EINVAL, ret;

	if (!udata) {
		pr_err("invalid input\n");
		goto end;
	}

	/* create switch device to update display modules */
	udata->sdev_display.name = "hdmi";
	rc = extcon_dev_register(&udata->sdev_display);
	if (rc) {
		pr_err("display switch registration failed\n");
		goto end;
	}

	udata->display_switch_registered = true;

	/* create switch device to update audio modules */
	udata->sdev_audio.name = "hdmi_audio";
	ret = extcon_dev_register(&udata->sdev_audio);
	if (ret) {
		pr_err("audio switch registration failed\n");
		goto end;
	}

	udata->audio_switch_registered = true;
end:
	return rc;
}

static int mdss_dba_get_vic_panel_info(struct mdss_dba_utils_data *udata,
					struct mdss_panel_info *pinfo)
{
	struct msm_hdmi_mode_timing_info timing;
	struct hdmi_util_ds_data ds_data;
	u32 h_total, v_total, vic = 0;

	if (!udata || !pinfo) {
		pr_err("%s: invalid input\n", __func__);
		return 0;
	}

	timing.active_h = pinfo->xres;
	timing.back_porch_h = pinfo->lcdc.h_back_porch;
	timing.front_porch_h = pinfo->lcdc.h_front_porch;
	timing.pulse_width_h = pinfo->lcdc.h_pulse_width;
	h_total = (timing.active_h + timing.back_porch_h +
		timing.front_porch_h + timing.pulse_width_h);

	timing.active_v = pinfo->yres;
	timing.back_porch_v = pinfo->lcdc.v_back_porch;
	timing.front_porch_v = pinfo->lcdc.v_front_porch;
	timing.pulse_width_v = pinfo->lcdc.v_pulse_width;
	v_total = (timing.active_v + timing.back_porch_v +
		timing.front_porch_v + timing.pulse_width_v);

	timing.refresh_rate = pinfo->mipi.frame_rate * 1000;
	timing.pixel_freq = (h_total * v_total *
				pinfo->mipi.frame_rate) / 1000;

	ds_data.ds_registered = true;
	ds_data.ds_max_clk = MSM_DBA_MAX_PCLK;

	vic = hdmi_get_video_id_code(&timing, &ds_data);
	pr_debug("%s: current vic code is %d\n", __func__, vic);

	return vic;
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
	video_cfg.pclk_khz = (unsigned long)pinfo->clk_rate / 1000;
	video_cfg.hdmi_mode = hdmi_edid_get_sink_mode(ud->edid_data);

	/* Calculate number of DSI lanes configured */
	video_cfg.num_of_input_lanes = 0;
	if (pinfo->mipi.data_lane0)
		video_cfg.num_of_input_lanes++;
	if (pinfo->mipi.data_lane1)
		video_cfg.num_of_input_lanes++;
	if (pinfo->mipi.data_lane2)
		video_cfg.num_of_input_lanes++;
	if (pinfo->mipi.data_lane3)
		video_cfg.num_of_input_lanes++;

	/* Get scan information from EDID */
	video_cfg.vic = mdss_dba_get_vic_panel_info(ud, pinfo);
	ud->current_vic = video_cfg.vic;
	video_cfg.scaninfo = hdmi_edid_get_sink_scaninfo(ud->edid_data,
							video_cfg.vic);
	if (ud->ops.video_on)
		ret = ud->ops.video_on(ud->dba_data, true, &video_cfg, 0);

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

end:
	return ret;
}

/**
 * mdss_dba_utils_hdcp_enable() - Allow clients to switch on HDCP.
 * @data: DBA utils instance which was allocated during registration
 * @enable: flag to enable or disable HDCP authentication
 *
 * This API is used to start the HDCP authentication process with the
 * device registered with DBA.
 */
void mdss_dba_utils_hdcp_enable(void *data, bool enable)
{
	struct mdss_dba_utils_data *ud = data;

	if (!ud) {
		pr_err("invalid input\n");
		return;
	}

	if (ud->ops.hdcp_enable)
		ud->ops.hdcp_enable(ud->dba_data, enable, enable, 0);
}

void mdss_dba_update_lane_cfg(struct mdss_panel_info *pinfo)
{
	struct mdss_dba_utils_data *dba_data;
	struct mdss_dba_timing_info *cfg_tbl;
	int i = 0, lanes;

	if (pinfo == NULL)
		return;

	/*
	 * Restore to default value from DT
	 * if resolution not found in
	 * supported resolutions
	 */
	lanes = pinfo->mipi.default_lanes;

	dba_data = (struct mdss_dba_utils_data *)(pinfo->dba_data);
	if (dba_data == NULL)
		goto lane_cfg;

	/* get adv supported timing info */
	cfg_tbl = (struct mdss_dba_timing_info *)(dba_data->timing_data);
	if (cfg_tbl == NULL)
		goto lane_cfg;

	while (cfg_tbl[i].xres != 0xffff) {
		if (cfg_tbl[i].xres == pinfo->xres &&
			cfg_tbl[i].yres == pinfo->yres &&
			cfg_tbl[i].bpp == pinfo->bpp &&
			cfg_tbl[i].fps == pinfo->mipi.frame_rate) {
			lanes = cfg_tbl[i].lanes;
			break;
		}
		i++;
	}

lane_cfg:
	switch (lanes) {
	case 1:
		pinfo->mipi.data_lane0 = 1;
		pinfo->mipi.data_lane1 = 0;
		pinfo->mipi.data_lane2 = 0;
		pinfo->mipi.data_lane3 = 0;
		break;
	case 2:
		pinfo->mipi.data_lane0 = 1;
		pinfo->mipi.data_lane1 = 1;
		pinfo->mipi.data_lane2 = 0;
		pinfo->mipi.data_lane3 = 0;
		break;
	case 3:
		pinfo->mipi.data_lane0 = 1;
		pinfo->mipi.data_lane1 = 1;
		pinfo->mipi.data_lane2 = 1;
		pinfo->mipi.data_lane3 = 0;
		break;
	case 4:
	default:
		pinfo->mipi.data_lane0 = 1;
		pinfo->mipi.data_lane1 = 1;
		pinfo->mipi.data_lane2 = 1;
		pinfo->mipi.data_lane3 = 1;
		break;
	}
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
	struct cec_abstract_init_data cec_abst_init_data;
	int ret = 0;

	if (!uid) {
		pr_err("invalid input\n");
		ret = -EINVAL;
		goto error;
	}

	udata = kzalloc(sizeof(*udata), GFP_KERNEL);
	if (!udata) {
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

	/* Initialize EDID feature */
	edid_init_data.kobj = uid->kobj;
	edid_init_data.ds_data.ds_registered = true;
	edid_init_data.ds_data.ds_max_clk = MSM_DBA_MAX_PCLK;
	edid_init_data.max_pclk_khz = MSM_DBA_MAX_PCLK;

	/* register with edid module for parsing edid buffer */
	udata->edid_data = hdmi_edid_init(&edid_init_data);
	if (!udata->edid_data) {
		pr_err("edid parser init failed\n");
		ret = -ENODEV;
		goto error;
	}

	/* update edid data to retrieve it back in edid parser */
	if (uid->pinfo) {
		uid->pinfo->edid_data = udata->edid_data;
		/* Initialize to default resolution */
		hdmi_edid_set_video_resolution(uid->pinfo->edid_data,
					DEFAULT_VIDEO_RESOLUTION, true);
	}

	/* get edid buffer from edid parser */
	udata->edid_buf = edid_init_data.buf;
	udata->edid_buf_size = edid_init_data.buf_size;

	/* Initialize cec abstract layer and get callbacks */
	udata->cops.send_msg = mdss_dba_utils_send_cec_msg;
	udata->cops.enable   = mdss_dba_utils_cec_enable;
	udata->cops.data     = udata;

	/* initialize cec abstraction module */
	cec_abst_init_data.kobj = uid->kobj;
	cec_abst_init_data.ops  = &udata->cops;
	cec_abst_init_data.cbs  = &udata->ccbs;

	udata->cec_abst_data = cec_abstract_init(&cec_abst_init_data);
	if (IS_ERR_OR_NULL(udata->cec_abst_data)) {
		pr_err("error initializing cec abstract module\n");
		ret = PTR_ERR(udata->cec_abst_data);
		goto error;
	}

	/* get the timing data for the adv chip */
	if (udata->ops.get_supp_timing_info)
		udata->timing_data = udata->ops.get_supp_timing_info();
	else
		udata->timing_data = NULL;

	/* update cec data to retrieve it back in cec abstract module */
	if (uid->pinfo) {
		uid->pinfo->is_cec_supported = true;
		uid->pinfo->cec_data = udata->cec_abst_data;

		/*
		 * TODO: Currently there is no support from HAL to send
		 * HPD events to driver for usecase where bridge chip
		 * is used as primary panel. Once support is added remove
		 * this explicit calls to bridge chip driver.
		 */
		if (!uid->pinfo->is_pluggable) {
			if (udata->ops.power_on && !(uid->cont_splash_enabled))
				udata->ops.power_on(udata->dba_data, true, 0);
			if (udata->ops.check_hpd)
				udata->ops.check_hpd(udata->dba_data, 0);
		} else {
			/* register display and audio switch devices */
			ret = mdss_dba_utils_init_switch_dev(udata,
				uid->fb_node);
			if (ret) {
				pr_err("switch dev registration failed\n");
				goto error;
			}
		}
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

	if (!IS_ERR_OR_NULL(udata->cec_abst_data))
		cec_abstract_deinit(udata->cec_abst_data);

	if (udata->edid_data)
		hdmi_edid_deinit(udata->edid_data);

	if (udata->pinfo) {
		udata->pinfo->edid_data = NULL;
		udata->pinfo->is_cec_supported = false;
	}

	if (udata->audio_switch_registered)
		extcon_dev_unregister(&udata->sdev_audio);

	if (udata->display_switch_registered)
		extcon_dev_unregister(&udata->sdev_display);

	if (udata->kobj)
		mdss_dba_utils_sysfs_remove(udata->kobj);

	if (IS_ENABLED(CONFIG_MSM_DBA)) {
		if (!IS_ERR_OR_NULL(udata->dba_data))
			msm_dba_deregister_client(udata->dba_data);
	}

	kfree(udata);
}
