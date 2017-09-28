/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#include <linux/of_platform.h>
#include <linux/msm_ext_display.h>

struct msm_ext_disp_list {
	struct msm_ext_disp_init_data *data;
	struct list_head list;
};

struct msm_ext_disp {
	struct msm_ext_disp_data ext_disp_data;
	struct platform_device *pdev;
	enum msm_ext_disp_type current_disp;
	struct msm_ext_disp_audio_codec_ops *ops;
	struct extcon_dev audio_sdev;
	bool audio_session_on;
	struct list_head display_list;
	struct mutex lock;
};

static const unsigned int msm_ext_disp_supported_cable[] = {
	EXTCON_DISP_DP,
	EXTCON_DISP_HDMI,
	EXTCON_NONE,
};

static int msm_ext_disp_extcon_register(struct msm_ext_disp *ext_disp)
{
	int ret = 0;

	if (!ext_disp) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	memset(&ext_disp->audio_sdev, 0x0, sizeof(ext_disp->audio_sdev));
	ext_disp->audio_sdev.supported_cable = msm_ext_disp_supported_cable;
	ext_disp->audio_sdev.dev.parent = &ext_disp->pdev->dev;
	ret = extcon_dev_register(&ext_disp->audio_sdev);
	if (ret) {
		pr_err("audio registration failed");
		return ret;
	}

	pr_debug("extcon registration done\n");

	return ret;
}

static void msm_ext_disp_extcon_unregister(struct msm_ext_disp *ext_disp)
{
	if (!ext_disp) {
		pr_err("Invalid params\n");
		return;
	}

	extcon_dev_unregister(&ext_disp->audio_sdev);
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

	pr_debug("Added new display (%s)\n", msm_ext_disp_name(data->type));

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
			*data = node->data;
			break;
		}
	}

	if (!*data) {
		pr_err("Display not found (%s)\n", msm_ext_disp_name(type));
		ret = -ENODEV;
	}
end:
	return ret;
}

static int msm_ext_disp_process_audio(struct msm_ext_disp *ext_disp,
		enum msm_ext_disp_type type,
		enum msm_ext_disp_cable_state new_state)
{
	int ret = 0;
	int state;

	state = ext_disp->audio_sdev.state;
	ret = extcon_set_state_sync(&ext_disp->audio_sdev,
			ext_disp->current_disp, !!new_state);

	pr_debug("Audio state %s %d\n",
			ext_disp->audio_sdev.state == state ?
			"is same" : "switched to",
			ext_disp->audio_sdev.state);

	return ret;
}

static struct msm_ext_disp *msm_ext_disp_validate_and_get(
		struct platform_device *pdev,
		enum msm_ext_disp_type type,
		enum msm_ext_disp_cable_state state)
{
	struct msm_ext_disp_data *ext_disp_data;
	struct msm_ext_disp *ext_disp;

	if (!pdev) {
		pr_err("invalid platform device\n");
		goto err;
	}

	ext_disp_data = platform_get_drvdata(pdev);
	if (!ext_disp_data) {
		pr_err("invalid drvdata\n");
		goto err;
	}

	ext_disp = container_of(ext_disp_data,
			struct msm_ext_disp, ext_disp_data);

	if (state < EXT_DISPLAY_CABLE_DISCONNECT ||
			state >= EXT_DISPLAY_CABLE_STATE_MAX) {
		pr_err("invalid HPD state (%d)\n", state);
		goto err;
	}

	if (state == EXT_DISPLAY_CABLE_CONNECT) {
		if (ext_disp->current_disp != EXT_DISPLAY_TYPE_MAX &&
		    ext_disp->current_disp != type) {
			pr_err("invalid interface call\n");
			goto err;
		}
	} else {
		if (ext_disp->current_disp == EXT_DISPLAY_TYPE_MAX ||
		    ext_disp->current_disp != type) {
			pr_err("invalid interface call\n");
			goto err;
		}
	}
	return ext_disp;
err:
	return ERR_PTR(-EINVAL);
}

static int msm_ext_disp_update_audio_ops(struct msm_ext_disp *ext_disp,
		enum msm_ext_disp_type type,
		enum msm_ext_disp_cable_state state)
{
	int ret = 0;
	struct msm_ext_disp_init_data *data = NULL;

	ret = msm_ext_disp_get_intf_data(ext_disp, type, &data);
	if (ret || !data) {
		pr_err("interface %s not found\n", msm_ext_disp_name(type));
		goto end;
	}

	if (!ext_disp->ops) {
		pr_err("codec ops not registered\n");
		ret = -EINVAL;
		goto end;
	}

	if (state == EXT_DISPLAY_CABLE_CONNECT) {
		/* connect codec with interface */
		*ext_disp->ops = data->codec_ops;

		/* update pdev for interface to use */
		ext_disp->ext_disp_data.intf_pdev = data->pdev;
		ext_disp->ext_disp_data.intf_data = data->intf_data;

		ext_disp->current_disp = type;

		pr_debug("codec ops set for %s\n", msm_ext_disp_name(type));
	} else if (state == EXT_DISPLAY_CABLE_DISCONNECT) {
		*ext_disp->ops = (struct msm_ext_disp_audio_codec_ops){NULL};
		ext_disp->current_disp = EXT_DISPLAY_TYPE_MAX;

		pr_debug("codec ops cleared for %s\n", msm_ext_disp_name(type));
	}
end:
	return ret;
}

static int msm_ext_disp_audio_config(struct platform_device *pdev,
		enum msm_ext_disp_type type,
		enum msm_ext_disp_cable_state state)
{
	int ret = 0;
	struct msm_ext_disp *ext_disp;

	ext_disp = msm_ext_disp_validate_and_get(pdev, type, state);
	if (IS_ERR(ext_disp)) {
		ret = PTR_ERR(ext_disp);
		goto end;
	}

	mutex_lock(&ext_disp->lock);
	ret = msm_ext_disp_update_audio_ops(ext_disp, type, state);
	mutex_unlock(&ext_disp->lock);
end:
	return ret;
}

static int msm_ext_disp_audio_notify(struct platform_device *pdev,
		enum msm_ext_disp_type type,
		enum msm_ext_disp_cable_state state)
{
	int ret = 0;
	struct msm_ext_disp *ext_disp;

	ext_disp = msm_ext_disp_validate_and_get(pdev, type, state);
	if (IS_ERR(ext_disp)) {
		ret = PTR_ERR(ext_disp);
		goto end;
	}

	mutex_lock(&ext_disp->lock);
	ret = msm_ext_disp_process_audio(ext_disp, type, state);
	mutex_unlock(&ext_disp->lock);
end:
	return ret;
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
EXPORT_SYMBOL(msm_ext_disp_register_audio_codec);

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

	ret = msm_ext_disp_get_intf_data(ext_disp, init_data->type, &data);
	if (!ret) {
		pr_err("%s already registered\n",
			msm_ext_disp_name(init_data->type));
		goto end;
	}

	ret = msm_ext_disp_add_intf_data(ext_disp, init_data);
	if (ret)
		goto end;

	init_data->intf_ops.audio_config = msm_ext_disp_audio_config;
	init_data->intf_ops.audio_notify = msm_ext_disp_audio_notify;

	pr_debug("%s registered\n", msm_ext_disp_name(init_data->type));

	mutex_unlock(&ext_disp->lock);

	return ret;

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

	platform_set_drvdata(pdev, &ext_disp->ext_disp_data);
	ext_disp->pdev = pdev;

	ret = msm_ext_disp_extcon_register(ext_disp);
	if (ret)
		goto extcon_dev_failure;

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
	msm_ext_disp_extcon_unregister(ext_disp);
extcon_dev_failure:
	devm_kfree(&ext_disp->pdev->dev, ext_disp);
end:
	return ret;
}

static int msm_ext_disp_remove(struct platform_device *pdev)
{
	int ret = 0;
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

	msm_ext_disp_extcon_unregister(ext_disp);

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

subsys_initcall(msm_ext_disp_init);
module_exit(msm_ext_disp_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM External Display");
