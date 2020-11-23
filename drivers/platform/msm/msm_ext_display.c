// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/iopoll.h>
#include <linux/types.h>
#include <linux/of_platform.h>
#include <linux/extcon-provider.h>
#include <linux/msm_ext_display.h>
#include <linux/extcon-provider.h>

struct msm_ext_disp_list {
	struct msm_ext_disp_init_data *data;
	struct list_head list;
};

struct msm_ext_disp {
	struct msm_ext_disp_data ext_disp_data;
	struct platform_device *pdev;
	struct msm_ext_disp_codec_id current_codec;
	struct msm_ext_disp_audio_codec_ops *ops;
	struct extcon_dev *audio_sdev[MSM_EXT_DISP_MAX_CODECS];
	bool audio_session_on;
	struct list_head display_list;
	struct mutex lock;
	bool update_audio;
};

static const unsigned int msm_ext_disp_supported_cable[] = {
	EXTCON_DISP_DP,
	EXTCON_DISP_HDMI,
	EXTCON_NONE,
};

static int msm_ext_disp_extcon_register(struct msm_ext_disp *ext_disp, int id)
{
	int ret = 0;

	if (!ext_disp || !ext_disp->pdev || id >= MSM_EXT_DISP_MAX_CODECS) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	ext_disp->audio_sdev[id] = devm_extcon_dev_allocate(
			&ext_disp->pdev->dev,
			msm_ext_disp_supported_cable);
	if (IS_ERR(ext_disp->audio_sdev[id]))
		return PTR_ERR(ext_disp->audio_sdev[id]);

	ret = devm_extcon_dev_register(&ext_disp->pdev->dev,
		ext_disp->audio_sdev[id]);
	if (ret) {
		pr_err("audio registration failed\n");
		return ret;
	}

	pr_debug("extcon registration done\n");

	return ret;
}

static void msm_ext_disp_extcon_unregister(struct msm_ext_disp *ext_disp,
		int id)
{
	if (!ext_disp || !ext_disp->pdev || id >= MSM_EXT_DISP_MAX_CODECS) {
		pr_err("Invalid params\n");
		return;
	}

	devm_extcon_dev_unregister(&ext_disp->pdev->dev,
			ext_disp->audio_sdev[id]);
}

static const char *msm_ext_disp_name(enum msm_ext_disp_type type)
{
	switch (type) {
	case EXT_DISPLAY_TYPE_HDMI:
		return "EXT_DISPLAY_TYPE_HDMI";
	case EXT_DISPLAY_TYPE_DP:
		return "EXT_DISPLAY_TYPE_DP";
	default: return "???";
	}
}

static int msm_ext_disp_add_intf_data(struct msm_ext_disp *ext_disp,
		struct msm_ext_disp_init_data *data)
{
	struct msm_ext_disp_list *node;

	if (!ext_disp || !data) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	node->data = data;

	list_add(&node->list, &ext_disp->display_list);

	pr_debug("Added new display (%s) ctld (%d) stream (%d)\n",
		msm_ext_disp_name(data->codec.type),
		data->codec.ctrl_id, data->codec.stream_id);

	return 0;
}

static int msm_ext_disp_remove_intf_data(struct msm_ext_disp *ext_disp,
		struct msm_ext_disp_init_data *data)
{
	struct msm_ext_disp_list *node;
	struct list_head *pos = NULL;

	if (!ext_disp || !data) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	list_for_each(pos, &ext_disp->display_list) {
		node = list_entry(pos, struct msm_ext_disp_list, list);
		if (node->data == data) {
			list_del(pos);
			pr_debug("Deleted the intf data\n");
			kfree(node);
			return 0;
		}
	}

	pr_debug("Intf data not present for delete op\n");

	return 0;
}

static int msm_ext_disp_get_intf_data(struct msm_ext_disp *ext_disp,
		struct msm_ext_disp_codec_id *codec,
		struct msm_ext_disp_init_data **data)
{
	int ret = 0;
	struct msm_ext_disp_list *node;
	struct list_head *position = NULL;

	if (!ext_disp || !data || !codec) {
		pr_err("Invalid params\n");
		ret = -EINVAL;
		goto end;
	}

	*data = NULL;
	list_for_each(position, &ext_disp->display_list) {
		node = list_entry(position, struct msm_ext_disp_list, list);
		if (node->data->codec.type == codec->type &&
			node->data->codec.stream_id == codec->stream_id &&
			node->data->codec.ctrl_id == codec->ctrl_id) {
			*data = node->data;
			break;
		}
	}

	if (!*data)
		ret = -ENODEV;
end:
	return ret;
}

static int msm_ext_disp_process_audio(struct msm_ext_disp *ext_disp,
		struct msm_ext_disp_codec_id *codec,
		enum msm_ext_disp_cable_state new_state)
{
	int ret = 0;
	int state;
	struct extcon_dev *audio_sdev;

	if (!ext_disp->ops) {
		pr_err("codec not registered, skip notification\n");
		ret = -EPERM;
		goto end;
	}

	audio_sdev = ext_disp->audio_sdev[codec->stream_id];

	state = extcon_get_state(audio_sdev, codec->type);
	if (state == !!new_state) {
		ret = -EEXIST;
		pr_debug("same state\n");
		goto end;
	}

	ret = extcon_set_state_sync(audio_sdev,
			codec->type, !!new_state);
	if (ret)
		pr_err("Failed to set state. Error = %d\n", ret);
	else
		pr_debug("state changed to %d\n", new_state);

end:
	return ret;
}

static struct msm_ext_disp *msm_ext_disp_validate_and_get(
		struct platform_device *pdev,
		struct msm_ext_disp_codec_id *codec,
		enum msm_ext_disp_cable_state state)
{
	struct msm_ext_disp_data *ext_disp_data;
	struct msm_ext_disp *ext_disp;

	if (!pdev) {
		pr_err("invalid platform device\n");
		goto err;
	}

	if (!codec ||
		codec->type >= EXT_DISPLAY_TYPE_MAX ||
		codec->ctrl_id != 0 ||
		codec->stream_id >= MSM_EXT_DISP_MAX_CODECS) {
		pr_err("invalid display codec id\n");
		goto err;
	}

	if (state < EXT_DISPLAY_CABLE_DISCONNECT ||
			state >= EXT_DISPLAY_CABLE_STATE_MAX) {
		pr_err("invalid HPD state (%d)\n", state);
		goto err;
	}

	ext_disp_data = platform_get_drvdata(pdev);
	if (!ext_disp_data) {
		pr_err("invalid drvdata\n");
		goto err;
	}

	ext_disp = container_of(ext_disp_data,
			struct msm_ext_disp, ext_disp_data);

	return ext_disp;
err:
	return ERR_PTR(-EINVAL);
}

static int msm_ext_disp_update_audio_ops(struct msm_ext_disp *ext_disp,
		struct msm_ext_disp_codec_id *codec)
{
	int ret = 0;
	struct msm_ext_disp_init_data *data = NULL;

	ret = msm_ext_disp_get_intf_data(ext_disp, codec, &data);
	if (ret || !data) {
		pr_err("Display not found (%s) ctld (%d) stream (%d)\n",
			msm_ext_disp_name(codec->type),
			codec->ctrl_id, codec->stream_id);
		goto end;
	}

	if (ext_disp->ops) {
		*ext_disp->ops = data->codec_ops;
		ext_disp->current_codec = *codec;

		/* update pdev for interface to use */
		ext_disp->ext_disp_data.intf_pdev = data->pdev;
		ext_disp->ext_disp_data.intf_data = data->intf_data;
	}

end:
	return ret;
}

static int msm_ext_disp_audio_config(struct platform_device *pdev,
		struct msm_ext_disp_codec_id *codec,
		enum msm_ext_disp_cable_state state)
{
	int ret = 0;
	struct msm_ext_disp *ext_disp;

	ext_disp = msm_ext_disp_validate_and_get(pdev, codec, state);
	if (IS_ERR(ext_disp)) {
		ret = PTR_ERR(ext_disp);
		goto end;
	}

	if (state == EXT_DISPLAY_CABLE_CONNECT) {
		ret = msm_ext_disp_select_audio_codec(pdev, codec);
	} else {
		mutex_lock(&ext_disp->lock);
		if (ext_disp->ops)
			memset(ext_disp->ops, 0, sizeof(*ext_disp->ops));

		pr_debug("codec ops cleared for %s\n",
			msm_ext_disp_name(ext_disp->current_codec.type));

		ext_disp->current_codec.type = EXT_DISPLAY_TYPE_MAX;
		mutex_unlock(&ext_disp->lock);
	}
end:
	return ret;
}

static int msm_ext_disp_audio_notify(struct platform_device *pdev,
		struct msm_ext_disp_codec_id *codec,
		enum msm_ext_disp_cable_state state)
{
	int ret = 0;
	struct msm_ext_disp *ext_disp;

	ext_disp = msm_ext_disp_validate_and_get(pdev, codec, state);
	if (IS_ERR(ext_disp)) {
		ret = PTR_ERR(ext_disp);
		goto end;
	}

	mutex_lock(&ext_disp->lock);
	ret = msm_ext_disp_process_audio(ext_disp, codec, state);
	mutex_unlock(&ext_disp->lock);
end:
	return ret;
}

static void msm_ext_disp_ready_for_display(struct msm_ext_disp *ext_disp)
{
	int ret;
	struct msm_ext_disp_init_data *data = NULL;

	if (!ext_disp) {
		pr_err("invalid input\n");
		return;
	}

	ret = msm_ext_disp_get_intf_data(ext_disp,
			&ext_disp->current_codec, &data);
	if (ret) {
		pr_err("%s not found\n",
			msm_ext_disp_name(ext_disp->current_codec.type));
		return;
	}

	*ext_disp->ops = data->codec_ops;
	data->codec_ops.ready(ext_disp->pdev);
}

int msm_hdmi_register_audio_codec(struct platform_device *pdev,
		struct msm_ext_disp_audio_codec_ops *ops)
{
	return msm_ext_disp_register_audio_codec(pdev, ops);
}

/**
 * Register audio codec ops to display driver
 * for HDMI/Display Port usecase support.
 *
 * @return 0 on success, negative value on error
 *
 */
int msm_ext_disp_register_audio_codec(struct platform_device *pdev,
		struct msm_ext_disp_audio_codec_ops *ops)
{
	int ret = 0;
	struct msm_ext_disp *ext_disp = NULL;
	struct msm_ext_disp_data *ext_disp_data = NULL;

	if (!pdev || !ops) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	ext_disp_data = platform_get_drvdata(pdev);
	if (!ext_disp_data) {
		pr_err("Invalid drvdata\n");
		return -EINVAL;
	}

	ext_disp = container_of(ext_disp_data, struct msm_ext_disp,
				ext_disp_data);

	mutex_lock(&ext_disp->lock);

	if (ext_disp->ops) {
		pr_err("Codec already registered\n");
		ret = -EINVAL;
		goto end;
	}

	ext_disp->ops = ops;

	pr_debug("audio codec registered\n");

	if (ext_disp->update_audio) {
		ext_disp->update_audio = false;
		msm_ext_disp_update_audio_ops(ext_disp,
				&ext_disp->current_codec);
		msm_ext_disp_process_audio(ext_disp, &ext_disp->current_codec,
				EXT_DISPLAY_CABLE_CONNECT);
	}

end:
	mutex_unlock(&ext_disp->lock);
	if (ext_disp->current_codec.type != EXT_DISPLAY_TYPE_MAX)
		msm_ext_disp_ready_for_display(ext_disp);

	return ret;
}
EXPORT_SYMBOL(msm_ext_disp_register_audio_codec);

int msm_ext_disp_select_audio_codec(struct platform_device *pdev,
		struct msm_ext_disp_codec_id *codec)
{
	int ret = 0;
	struct msm_ext_disp *ext_disp = NULL;
	struct msm_ext_disp_data *ext_disp_data = NULL;

	if (!pdev || !codec) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	ext_disp_data = platform_get_drvdata(pdev);
	if (!ext_disp_data) {
		pr_err("Invalid drvdata\n");
		return -EINVAL;
	}

	ext_disp = container_of(ext_disp_data, struct msm_ext_disp,
				ext_disp_data);

	mutex_lock(&ext_disp->lock);

	if (!ext_disp->ops) {
		pr_warn("Codec is not registered\n");
		ext_disp->update_audio = true;
		ext_disp->current_codec = *codec;
		ret = -EINVAL;
		goto end;
	}

	ret = msm_ext_disp_update_audio_ops(ext_disp, codec);

end:
	mutex_unlock(&ext_disp->lock);

	return ret;
}
EXPORT_SYMBOL(msm_ext_disp_select_audio_codec);

static int msm_ext_disp_validate_intf(struct msm_ext_disp_init_data *init_data)
{
	struct msm_ext_disp_audio_codec_ops *ops;

	if (!init_data) {
		pr_err("Invalid init_data\n");
		return -EINVAL;
	}

	if (!init_data->pdev) {
		pr_err("Invalid display intf pdev\n");
		return -EINVAL;
	}

	if (init_data->codec.type >= EXT_DISPLAY_TYPE_MAX ||
		init_data->codec.ctrl_id != 0 ||
		init_data->codec.stream_id >= MSM_EXT_DISP_MAX_CODECS) {
		pr_err("Invalid codec info type(%d), ctrl(%d) stream(%d)\n",
				init_data->codec.type,
				init_data->codec.ctrl_id,
				init_data->codec.stream_id);
		return -EINVAL;
	}

	ops = &init_data->codec_ops;

	if (!ops->audio_info_setup || !ops->get_audio_edid_blk ||
			!ops->cable_status || !ops->get_intf_id ||
			!ops->teardown_done || !ops->acknowledge ||
			!ops->ready) {
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
	struct msm_ext_disp_data *ext_disp_data = NULL;

	if (!pdev || !init_data) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	ext_disp_data = platform_get_drvdata(pdev);
	if (!ext_disp_data) {
		pr_err("Invalid drvdata\n");
		return -EINVAL;
	}

	ext_disp = container_of(ext_disp_data, struct msm_ext_disp,
				ext_disp_data);

	mutex_lock(&ext_disp->lock);

	ret = msm_ext_disp_validate_intf(init_data);
	if (ret)
		goto end;

	ret = msm_ext_disp_get_intf_data(ext_disp, &init_data->codec, &data);
	if (!ret) {
		pr_err("%s already registered. ctrl(%d) stream(%d)\n",
			msm_ext_disp_name(init_data->codec.type),
			init_data->codec.ctrl_id,
			init_data->codec.stream_id);
		goto end;
	}

	ret = msm_ext_disp_add_intf_data(ext_disp, init_data);
	if (ret)
		goto end;

	init_data->intf_ops.audio_config = msm_ext_disp_audio_config;
	init_data->intf_ops.audio_notify = msm_ext_disp_audio_notify;

	pr_debug("%s registered. ctrl(%d) stream(%d)\n",
			msm_ext_disp_name(init_data->codec.type),
			init_data->codec.ctrl_id,
			init_data->codec.stream_id);
end:
	mutex_unlock(&ext_disp->lock);
	return ret;
}
EXPORT_SYMBOL(msm_ext_disp_register_intf);

int msm_ext_disp_deregister_intf(struct platform_device *pdev,
		struct msm_ext_disp_init_data *init_data)
{
	int ret = 0;
	struct msm_ext_disp *ext_disp = NULL;
	struct msm_ext_disp_data *ext_disp_data = NULL;

	if (!pdev || !init_data) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	ext_disp_data = platform_get_drvdata(pdev);
	if (!ext_disp_data) {
		pr_err("Invalid drvdata\n");
		return -EINVAL;
	}

	ext_disp = container_of(ext_disp_data, struct msm_ext_disp,
				ext_disp_data);

	mutex_lock(&ext_disp->lock);

	ret = msm_ext_disp_remove_intf_data(ext_disp, init_data);
	if (ret)
		goto end;

	init_data->intf_ops.audio_config = NULL;
	init_data->intf_ops.audio_notify = NULL;

	pr_debug("%s deregistered\n",
			msm_ext_disp_name(init_data->codec.type));
end:
	mutex_unlock(&ext_disp->lock);

	return ret;
}
EXPORT_SYMBOL(msm_ext_disp_deregister_intf);

static int msm_ext_disp_probe(struct platform_device *pdev)
{
	int ret = 0, id;
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

	platform_set_drvdata(pdev, &ext_disp->ext_disp_data);
	ext_disp->pdev = pdev;

	for (id = 0; id < MSM_EXT_DISP_MAX_CODECS; id++) {
		ret = msm_ext_disp_extcon_register(ext_disp, id);
		if (ret)
			goto child_node_failure;
	}

	ret = of_platform_populate(of_node, NULL, NULL, &pdev->dev);
	if (ret) {
		pr_err("Failed to add child devices. Error = %d\n", ret);
		goto child_node_failure;
	} else {
		pr_debug("%s: Added child devices.\n", __func__);
	}

	mutex_init(&ext_disp->lock);

	INIT_LIST_HEAD(&ext_disp->display_list);
	ext_disp->current_codec.type = EXT_DISPLAY_TYPE_MAX;
	ext_disp->update_audio = false;

	return ret;

child_node_failure:
	for (id = 0; id < MSM_EXT_DISP_MAX_CODECS; id++)
		msm_ext_disp_extcon_unregister(ext_disp, id);

	devm_kfree(&ext_disp->pdev->dev, ext_disp);
end:
	return ret;
}

static int msm_ext_disp_remove(struct platform_device *pdev)
{
	int ret = 0, id;
	struct msm_ext_disp *ext_disp = NULL;
	struct msm_ext_disp_data *ext_disp_data = NULL;

	if (!pdev) {
		pr_err("No platform device\n");
		ret = -ENODEV;
		goto end;
	}

	ext_disp_data = platform_get_drvdata(pdev);
	if (!ext_disp_data) {
		pr_err("No drvdata found\n");
		ret = -ENODEV;
		goto end;
	}

	ext_disp = container_of(ext_disp_data, struct msm_ext_disp,
				ext_disp_data);

	for (id = 0; id < MSM_EXT_DISP_MAX_CODECS; id++)
		msm_ext_disp_extcon_unregister(ext_disp, id);

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

subsys_initcall(msm_ext_disp_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM External Display");
