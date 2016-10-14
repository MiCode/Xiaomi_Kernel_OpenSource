/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/iopoll.h>
#include <linux/types.h>
#include <linux/switch.h>
#include <linux/of_platform.h>
#include <linux/msm_ext_display.h>

#include "mdss_hdmi_util.h"
#include "mdss_fb.h"

struct msm_ext_disp_list {
	struct msm_ext_disp_init_data *data;
	struct list_head list;
};

struct msm_ext_disp {
	struct platform_device *pdev;
	enum msm_ext_disp_type current_disp;
	struct msm_ext_disp_audio_codec_ops *ops;
	struct switch_dev hdmi_sdev;
	struct switch_dev audio_sdev;
	bool ack_enabled;
	atomic_t ack_pending;
	struct list_head display_list;
	struct mutex lock;
};

static int msm_ext_disp_get_intf_data(struct msm_ext_disp *ext_disp,
		enum msm_ext_disp_type type,
		struct msm_ext_disp_init_data **data);
static int msm_ext_disp_audio_ack(struct platform_device *pdev, u32 ack);

static int msm_ext_disp_switch_dev_register(struct msm_ext_disp *ext_disp)
{
	int ret = 0;

	if (!ext_disp) {
		pr_err("Invalid params\n");
		ret = -EINVAL;
		goto end;
	}

	memset(&ext_disp->hdmi_sdev, 0x0, sizeof(ext_disp->hdmi_sdev));
	ext_disp->hdmi_sdev.name = "hdmi";
	ret = switch_dev_register(&ext_disp->hdmi_sdev);
	if (ret) {
		pr_err("hdmi switch registration failed\n");
		goto end;
	}

	memset(&ext_disp->audio_sdev, 0x0, sizeof(ext_disp->audio_sdev));
	ext_disp->audio_sdev.name = "hdmi_audio";
	ret = switch_dev_register(&ext_disp->audio_sdev);
	if (ret) {
		pr_err("hdmi_audio switch registration failed");
		goto hdmi_audio_failure;
	}

	pr_debug("Display switch registration pass\n");

	return ret;

hdmi_audio_failure:
	switch_dev_unregister(&ext_disp->hdmi_sdev);
end:
	return ret;
}

static void msm_ext_disp_switch_dev_unregister(struct msm_ext_disp *ext_disp)
{
	if (!ext_disp) {
		pr_err("Invalid params\n");
		goto end;
	}

	switch_dev_unregister(&ext_disp->hdmi_sdev);
	switch_dev_unregister(&ext_disp->audio_sdev);

end:
	return;
}

static void msm_ext_disp_get_pdev_by_name(struct device *dev,
		const char *phandle, struct platform_device **pdev)
{
	struct device_node *pd_np;

	if (!dev) {
		pr_err("Invalid device\n");
		return;
	}

	if (!dev->of_node) {
		pr_err("Invalid of_node\n");
		return;
	}

	pd_np = of_parse_phandle(dev->of_node, phandle, 0);
	if (!pd_np) {
		pr_err("Cannot find %s dev\n", phandle);
		return;
	}

	*pdev = of_find_device_by_node(pd_np);
}

static void msm_ext_disp_get_fb_pdev(struct device *device,
		struct platform_device **fb_pdev)
{
	struct msm_fb_data_type *mfd = NULL;
	struct fb_info *fbi = dev_get_drvdata(device);

	if (!fbi) {
		pr_err("fb_info is null\n");
		return;
	}

	mfd = (struct msm_fb_data_type *)fbi->par;

	*fb_pdev = mfd->pdev;
}
static ssize_t msm_ext_disp_sysfs_wta_audio_cb(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ack, ret = 0;
	ssize_t size = strnlen(buf, PAGE_SIZE);
	const char *ext_phandle = "qcom,msm_ext_disp";
	struct platform_device *ext_pdev = NULL;
	const char *intf_phandle = "qcom,mdss-intf";
	struct platform_device *intf_pdev = NULL;
	struct platform_device *fb_pdev = NULL;

	ret = kstrtoint(buf, 10, &ack);
	if (ret) {
		pr_err("kstrtoint failed. ret=%d\n", ret);
		goto end;
	}

	msm_ext_disp_get_fb_pdev(dev, &fb_pdev);
	if (!fb_pdev) {
		pr_err("failed to get fb pdev\n");
		goto end;
	}

	msm_ext_disp_get_pdev_by_name(&fb_pdev->dev, intf_phandle, &intf_pdev);
	if (!intf_pdev) {
		pr_err("failed to get display intf pdev\n");
		goto end;
	}

	msm_ext_disp_get_pdev_by_name(&intf_pdev->dev, ext_phandle, &ext_pdev);
	if (!ext_pdev) {
		pr_err("failed to get ext_pdev\n");
		goto end;
	}

	ret = msm_ext_disp_audio_ack(ext_pdev, ack);
	if (ret)
		pr_err("Failed to process ack. ret=%d\n", ret);

end:
	return size;
}

static DEVICE_ATTR(hdmi_audio_cb, S_IWUSR, NULL,
		msm_ext_disp_sysfs_wta_audio_cb);

static struct attribute *msm_ext_disp_fs_attrs[] = {
	&dev_attr_hdmi_audio_cb.attr,
	NULL,
};

static struct attribute_group msm_ext_disp_fs_attrs_group = {
	.attrs = msm_ext_disp_fs_attrs,
};

static int msm_ext_disp_sysfs_create(struct msm_ext_disp_init_data *data)
{
	int ret = 0;

	if (!data || !data->kobj) {
		pr_err("Invalid params\n");
		ret = -EINVAL;
		goto end;
	}

	ret = sysfs_create_group(data->kobj, &msm_ext_disp_fs_attrs_group);
	if (ret)
		pr_err("Failed, ret=%d\n", ret);

end:
	return ret;
}

static void msm_ext_disp_sysfs_remove(struct msm_ext_disp_init_data *data)
{
	if (!data || !data->kobj) {
		pr_err("Invalid params\n");
		return;
	}

	sysfs_remove_group(data->kobj, &msm_ext_disp_fs_attrs_group);
}

static const char *msm_ext_disp_name(enum msm_ext_disp_type type)
{
	switch (type) {
	case EXT_DISPLAY_TYPE_HDMI:	return "EXT_DISPLAY_TYPE_HDMI";
	case EXT_DISPLAY_TYPE_DP:	return "EXT_DISPLAY_TYPE_DP";
	default: return "???";
	}
}

static int msm_ext_disp_add_intf_data(struct msm_ext_disp *ext_disp,
		struct msm_ext_disp_init_data *data)
{
	struct msm_ext_disp_list *node;

	if (!ext_disp && !data) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	node->data = data;
	list_add(&node->list, &ext_disp->display_list);

	pr_debug("Added new display (%s)\n",
			msm_ext_disp_name(data->type));

	return 0;
}

static int msm_ext_disp_get_intf_data(struct msm_ext_disp *ext_disp,
		enum msm_ext_disp_type type,
		struct msm_ext_disp_init_data **data)
{
	int ret = 0;
	struct msm_ext_disp_list *node;
	struct list_head *position = NULL;

	if (!ext_disp || !data || type < EXT_DISPLAY_TYPE_HDMI ||
			type >=  EXT_DISPLAY_TYPE_MAX) {
		pr_err("Invalid params\n");
		ret = -EINVAL;
		goto end;
	}

	*data = NULL;
	list_for_each(position, &ext_disp->display_list) {
		node = list_entry(position, struct msm_ext_disp_list, list);
		if (node->data->type == type) {
			pr_debug("Found display (%s)\n",
					msm_ext_disp_name(type));
			*data = node->data;
			break;
		}
	}

	if (!*data) {
		pr_debug("Display not found (%s)\n",
				msm_ext_disp_name(type));
		ret = -ENODEV;
	}

end:
	return ret;
}

static void msm_ext_disp_remove_intf_data(struct msm_ext_disp *ext_disp,
		enum msm_ext_disp_type type)
{
	struct msm_ext_disp_list *node;
	struct list_head *position = NULL;
	struct list_head *temp = NULL;

	if (!ext_disp) {
		pr_err("Invalid params\n");
		return;
	}

	list_for_each_safe(position, temp, &ext_disp->display_list) {
		node = list_entry(position, struct msm_ext_disp_list, list);
		if (node->data->type == type) {
			msm_ext_disp_sysfs_remove(node->data);
			list_del(&node->list);
			pr_debug("Removed display (%s)\n",
					msm_ext_disp_name(type));
			kfree(node);
			break;
		}
	}
}

static void msm_ext_disp_send_cable_notification(struct msm_ext_disp *ext_disp,
		enum msm_ext_disp_cable_state new_state)
{
	int state = EXT_DISPLAY_CABLE_STATE_MAX;

	if (!ext_disp) {
		pr_err("Invalid params\n");
		return;
	}

	state = ext_disp->hdmi_sdev.state;
	switch_set_state(&ext_disp->hdmi_sdev, !!new_state);

	pr_debug("Cable state %s %d\n",
			ext_disp->hdmi_sdev.state == state ?
			"is same" : "switched to",
			ext_disp->hdmi_sdev.state);
}

static int msm_ext_disp_hpd(struct platform_device *pdev,
		enum msm_ext_disp_type type,
		enum msm_ext_disp_cable_state state)
{
	int ret = 0;
	struct msm_ext_disp_init_data *data = NULL;
	struct msm_ext_disp *ext_disp = NULL;

	if (!pdev) {
		pr_err("Invalid platform device\n");
		return -EINVAL;
	}

	ext_disp = platform_get_drvdata(pdev);
	if (!ext_disp) {
		pr_err("Invalid drvdata\n");
		return -EINVAL;
	}

	mutex_lock(&ext_disp->lock);

	pr_debug("HPD for display (%s), NEW STATE = %d\n",
			msm_ext_disp_name(type), state);

	if (state < EXT_DISPLAY_CABLE_DISCONNECT ||
			state >= EXT_DISPLAY_CABLE_STATE_MAX) {
		pr_err("Invalid HPD state (%d)\n", state);
		ret = -EINVAL;
		goto end;
	}

	if ((state == EXT_DISPLAY_CABLE_CONNECT) &&
			(ext_disp->current_disp != EXT_DISPLAY_TYPE_MAX)) {
		pr_err("Display interface (%s) already connected\n",
				msm_ext_disp_name(ext_disp->current_disp));
		ret = -EINVAL;
		goto end;
	}

	if ((state == EXT_DISPLAY_CABLE_DISCONNECT) &&
			(ext_disp->current_disp != type)) {
		pr_err("Display interface (%s) is not connected\n",
				msm_ext_disp_name(type));
		ret = -EINVAL;
		goto end;
	}

	ret = msm_ext_disp_get_intf_data(ext_disp, type, &data);
	if (ret)
		goto end;

	if (state == EXT_DISPLAY_CABLE_CONNECT) {
		ext_disp->current_disp = data->type;
	} else if ((state == EXT_DISPLAY_CABLE_DISCONNECT) &&
			!ext_disp->ack_enabled) {
		if (ext_disp->ops) {
			ext_disp->ops->audio_info_setup = NULL;
			ext_disp->ops->get_audio_edid_blk = NULL;
			ext_disp->ops->cable_status = NULL;
			ext_disp->ops->get_intf_id = NULL;
			ext_disp->ops->teardown_done = NULL;
		}

		ext_disp->current_disp = EXT_DISPLAY_TYPE_MAX;
	}

	msm_ext_disp_send_cable_notification(ext_disp, state);

	pr_debug("Hpd (%d) for display (%s)\n", state,
			msm_ext_disp_name(type));

end:
	mutex_unlock(&ext_disp->lock);

	return ret;
}
static int msm_ext_disp_get_intf_data_helper(struct platform_device *pdev,
		struct msm_ext_disp_init_data **data)
{
	int ret = 0;
	struct msm_ext_disp *ext_disp = NULL;

	if (!pdev) {
		pr_err("No platform device\n");
		ret = -ENODEV;
		goto end;
	}

	ext_disp = platform_get_drvdata(pdev);
	if (!ext_disp) {
		pr_err("No drvdata found\n");
		ret = -ENODEV;
		goto end;
	}

	mutex_lock(&ext_disp->lock);

	if (ext_disp->current_disp == EXT_DISPLAY_TYPE_MAX) {
		ret = -EINVAL;
		pr_err("No display connected\n");
		goto error;
	}

	ret = msm_ext_disp_get_intf_data(ext_disp, ext_disp->current_disp,
			data);
	if (ret)
		goto error;
error:
	mutex_unlock(&ext_disp->lock);
end:
	return ret;
}
static int msm_ext_disp_cable_status(struct platform_device *pdev, u32 vote)
{
	int ret = 0;
	struct msm_ext_disp_init_data *data = NULL;

	ret = msm_ext_disp_get_intf_data_helper(pdev, &data);
	if (ret || !data)
		goto end;

	ret = data->codec_ops.cable_status(data->pdev, vote);

end:
	return ret;
}

static int msm_ext_disp_get_audio_edid_blk(struct platform_device *pdev,
	struct msm_ext_disp_audio_edid_blk *blk)
{
	int ret = 0;
	struct msm_ext_disp_init_data *data = NULL;

	ret = msm_ext_disp_get_intf_data_helper(pdev, &data);
	if (ret || !data)
		goto end;

	ret = data->codec_ops.get_audio_edid_blk(data->pdev, blk);

end:
	return ret;
}

static int msm_ext_disp_audio_info_setup(struct platform_device *pdev,
	struct msm_ext_disp_audio_setup_params *params)
{
	int ret = 0;
	struct msm_ext_disp_init_data *data = NULL;

	ret = msm_ext_disp_get_intf_data_helper(pdev, &data);
	if (ret || !data)
		goto end;

	ret = data->codec_ops.audio_info_setup(data->pdev, params);

end:
	return ret;
}

static void msm_ext_disp_teardown_done(struct platform_device *pdev)
{
	int ret = 0;
	struct msm_ext_disp_init_data *data = NULL;

	ret = msm_ext_disp_get_intf_data_helper(pdev, &data);
	if (ret || !data) {
		pr_err("invalid input");
		return;
	}

	data->codec_ops.teardown_done(data->pdev);
}

static int msm_ext_disp_get_intf_id(struct platform_device *pdev)
{
	int ret = 0;
	struct msm_ext_disp *ext_disp = NULL;

	if (!pdev) {
		pr_err("No platform device\n");
		ret = -ENODEV;
		goto end;
	}

	ext_disp = platform_get_drvdata(pdev);
	if (!ext_disp) {
		pr_err("No drvdata found\n");
		ret = -ENODEV;
		goto end;
	}

	mutex_lock(&ext_disp->lock);
	ret = ext_disp->current_disp;
	mutex_unlock(&ext_disp->lock);

end:
	return ret;
}

static int msm_ext_disp_notify(struct platform_device *pdev,
		enum msm_ext_disp_cable_state new_state)
{
	int ret = 0;
	int state = 0;
	bool switched;
	struct msm_ext_disp_init_data *data = NULL;
	struct msm_ext_disp *ext_disp = NULL;

	if (!pdev) {
		pr_err("Invalid platform device\n");
		return -EINVAL;
	}

	ext_disp = platform_get_drvdata(pdev);
	if (!ext_disp) {
		pr_err("Invalid drvdata\n");
		return -EINVAL;
	}

	mutex_lock(&ext_disp->lock);

	if (state < EXT_DISPLAY_CABLE_DISCONNECT ||
			state >= EXT_DISPLAY_CABLE_STATE_MAX) {
		pr_err("Invalid state (%d)\n", state);
		ret = -EINVAL;
		goto end;
	}

	state = ext_disp->audio_sdev.state;
	if (state == new_state)
		goto end;

	if (ext_disp->ack_enabled &&
		atomic_read(&ext_disp->ack_pending)) {
		ret = -EINVAL;
		pr_err("%s ack pending, not notifying %s\n",
			state ? "connect" : "disconnect",
			new_state ? "connect" : "disconnect");
		goto end;
	}

	ret = msm_ext_disp_get_intf_data(ext_disp, ext_disp->current_disp,
			&data);
	if (ret)
		goto end;

	if (new_state == EXT_DISPLAY_CABLE_CONNECT && ext_disp->ops) {
		ext_disp->ops->audio_info_setup =
			msm_ext_disp_audio_info_setup;
		ext_disp->ops->get_audio_edid_blk =
			msm_ext_disp_get_audio_edid_blk;
		ext_disp->ops->cable_status =
			msm_ext_disp_cable_status;
		ext_disp->ops->get_intf_id =
			msm_ext_disp_get_intf_id;
		ext_disp->ops->teardown_done =
			msm_ext_disp_teardown_done;
	}

	switch_set_state(&ext_disp->audio_sdev, (int)new_state);
	switched = ext_disp->audio_sdev.state != state;

	if (ext_disp->ack_enabled && switched)
		atomic_set(&ext_disp->ack_pending, 1);

	pr_debug("audio %s %s\n", switched ? "switched to" : "same as",
		ext_disp->audio_sdev.state ? "HDMI" : "SPKR");

end:
	mutex_unlock(&ext_disp->lock);

	return ret;
}

static int msm_ext_disp_audio_ack(struct platform_device *pdev, u32 ack)
{
	u32 ack_hpd;
	u32 hpd;
	int ret = 0;
	struct msm_ext_disp *ext_disp = NULL;

	if (!pdev) {
		pr_err("Invalid platform device\n");
		return -EINVAL;
	}

	ext_disp = platform_get_drvdata(pdev);
	if (!ext_disp) {
		pr_err("Invalid drvdata\n");
		return -EINVAL;
	}

	mutex_lock(&ext_disp->lock);

	hpd = ext_disp->current_disp != EXT_DISPLAY_TYPE_MAX;

	if (ack & AUDIO_ACK_SET_ENABLE) {
		ext_disp->ack_enabled = ack & AUDIO_ACK_ENABLE ?
			true : false;

		pr_debug("audio ack feature %s\n",
			ext_disp->ack_enabled ? "enabled" : "disabled");
		goto end;
	}

	if (!ext_disp->ack_enabled)
		goto end;

	atomic_set(&ext_disp->ack_pending, 0);

	ack_hpd = ack & AUDIO_ACK_CONNECT;

	pr_debug("acknowledging %s\n",
			ack_hpd ? "connect" : "disconnect");

	/**
	 * If the ack feature is enabled and we receive an ack for
	 * disconnect then we reset the current display state to
	 * empty.
	 */
	if (!ack_hpd) {
		if (ext_disp->ops) {
			ext_disp->ops->audio_info_setup = NULL;
			ext_disp->ops->get_audio_edid_blk = NULL;
			ext_disp->ops->cable_status = NULL;
			ext_disp->ops->get_intf_id = NULL;
			ext_disp->ops->teardown_done = NULL;
		}

		ext_disp->current_disp = EXT_DISPLAY_TYPE_MAX;
	}

	if (ack_hpd != hpd) {
		pr_err("unbalanced audio state, ack %d, hpd %d\n",
			ack_hpd, hpd);

		mutex_unlock(&ext_disp->lock);

		ret = msm_ext_disp_notify(pdev, hpd);

		return ret;
	}

end:
	mutex_unlock(&ext_disp->lock);

	return ret;
}

int msm_hdmi_register_audio_codec(struct platform_device *pdev,
	struct msm_ext_disp_audio_codec_ops *ops)
{
	return msm_ext_disp_register_audio_codec(pdev, ops);
}

int msm_ext_disp_register_audio_codec(struct platform_device *pdev,
		struct msm_ext_disp_audio_codec_ops *ops)
{
	int ret = 0;
	struct msm_ext_disp *ext_disp = NULL;

	if (!pdev || !ops) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	ext_disp = platform_get_drvdata(pdev);
	if (!ext_disp) {
		pr_err("Invalid drvdata\n");
		return -EINVAL;
	}

	mutex_lock(&ext_disp->lock);

	if ((ext_disp->current_disp != EXT_DISPLAY_TYPE_MAX)
			&& ext_disp->ops) {
		pr_err("Codec already registered\n");
		ret = -EINVAL;
		goto end;
	}

	ext_disp->ops = ops;

	pr_debug("audio codec registered\n");

end:
	mutex_unlock(&ext_disp->lock);

	return ret;
}

static int msm_ext_disp_validate_intf(struct msm_ext_disp_init_data *init_data)
{
	if (!init_data) {
		pr_err("Invalid init_data\n");
		return -EINVAL;
	}

	if (!init_data->pdev) {
		pr_err("Invalid display intf pdev\n");
		return -EINVAL;
	}

	if (!init_data->kobj) {
		pr_err("Invalid display intf kobj\n");
		return -EINVAL;
	}

	if (!init_data->codec_ops.get_audio_edid_blk ||
			!init_data->codec_ops.cable_status ||
			!init_data->codec_ops.audio_info_setup) {
		pr_err("Invalid codec operation pointers\n");
		return -EINVAL;
	}

	return 0;
}

int msm_ext_disp_register_intf(struct platform_device *pdev,
		struct msm_ext_disp_init_data *init_data)
{
	int ret = 0;
	struct msm_ext_disp_init_data *data = NULL;
	struct msm_ext_disp *ext_disp = NULL;

	if (!pdev || !init_data) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	ext_disp = platform_get_drvdata(pdev);
	if (!ext_disp) {
		pr_err("Invalid drvdata\n");
		return -EINVAL;
	}

	mutex_lock(&ext_disp->lock);

	ret = msm_ext_disp_validate_intf(init_data);
	if (ret)
		goto end;

	ret = msm_ext_disp_get_intf_data(ext_disp, init_data->type, &data);
	if (!ret) {
		pr_debug("Display (%s) already registered\n",
				msm_ext_disp_name(init_data->type));
		goto end;
	}

	ret = msm_ext_disp_add_intf_data(ext_disp, init_data);
	if (ret)
		goto end;

	ret = msm_ext_disp_sysfs_create(init_data);
	if (ret)
		goto sysfs_failure;

	init_data->intf_ops.hpd = msm_ext_disp_hpd;
	init_data->intf_ops.notify = msm_ext_disp_notify;

	pr_debug("Display (%s) registered\n",
			msm_ext_disp_name(init_data->type));

	mutex_unlock(&ext_disp->lock);

	return ret;

sysfs_failure:
	msm_ext_disp_remove_intf_data(ext_disp, init_data->type);
end:
	mutex_unlock(&ext_disp->lock);

	return ret;
}

static int msm_ext_disp_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *of_node = NULL;
	struct msm_ext_disp *ext_disp = NULL;

	if (!pdev) {
		pr_err("No platform device found\n");
		ret = -ENODEV;
		goto end;
	}

	of_node = pdev->dev.of_node;
	if (!of_node) {
		pr_err("No device node found\n");
		ret = -ENODEV;
		goto end;
	}

	ext_disp = devm_kzalloc(&pdev->dev, sizeof(*ext_disp), GFP_KERNEL);
	if (!ext_disp) {
		ret = -ENOMEM;
		goto end;
	}

	platform_set_drvdata(pdev, ext_disp);
	ext_disp->pdev = pdev;

	ret = msm_ext_disp_switch_dev_register(ext_disp);
	if (ret)
		goto switch_dev_failure;

	ret = of_platform_populate(of_node, NULL, NULL, &pdev->dev);
	if (ret) {
		pr_err("Failed to add child devices. Error = %d\n", ret);
		goto child_node_failure;
	} else {
		pr_debug("%s: Added child devices.\n", __func__);
	}

	mutex_init(&ext_disp->lock);

	INIT_LIST_HEAD(&ext_disp->display_list);
	ext_disp->current_disp = EXT_DISPLAY_TYPE_MAX;

	return ret;

child_node_failure:
	msm_ext_disp_switch_dev_unregister(ext_disp);
switch_dev_failure:
	devm_kfree(&ext_disp->pdev->dev, ext_disp);
end:
	return ret;
}

static int msm_ext_disp_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct msm_ext_disp *ext_disp = NULL;

	if (!pdev) {
		pr_err("No platform device\n");
		ret = -ENODEV;
		goto end;
	}

	ext_disp = platform_get_drvdata(pdev);
	if (!ext_disp) {
		pr_err("No drvdata found\n");
		ret = -ENODEV;
		goto end;
	}

	msm_ext_disp_switch_dev_unregister(ext_disp);

	mutex_destroy(&ext_disp->lock);
	devm_kfree(&ext_disp->pdev->dev, ext_disp);

end:
	return ret;
}

static const struct of_device_id msm_ext_dt_match[] = {
	{.compatible = "qcom,msm-ext-disp",},
	{ /* Sentinel */ },
};
MODULE_DEVICE_TABLE(of, msm_ext_dt_match);

static struct platform_driver this_driver = {
	.probe = msm_ext_disp_probe,
	.remove = msm_ext_disp_remove,
	.driver = {
		.name = "msm-ext-disp",
		.of_match_table = msm_ext_dt_match,
	},
};

static int __init msm_ext_disp_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&this_driver);
	if (ret)
		pr_err("failed, ret = %d\n", ret);

	return ret;
}

static void __exit msm_ext_disp_exit(void)
{
	platform_driver_unregister(&this_driver);
}

module_init(msm_ext_disp_init);
module_exit(msm_ext_disp_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM External Display");

