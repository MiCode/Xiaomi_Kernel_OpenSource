/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/apr_audio-v2.h>
#include <sound/q6afe-v2.h>
#include <sound/msm-dai-q6-v2.h>
#include <sound/pcm_params.h>
#include <mach/clk.h>

enum {
	STATUS_PORT_STARTED, /* track if AFE port has started */
	STATUS_MAX
};

struct msm_dai_q6_dai_data {
	DECLARE_BITMAP(status_mask, STATUS_MAX);
	u32 rate;
	u32 channels;
	union afe_port_config port_config;
};

static struct clk *pcm_clk;
static DEFINE_MUTEX(aux_pcm_mutex);
static int aux_pcm_count;

static int msm_dai_q6_auxpcm_hw_params(
				struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	struct msm_dai_auxpcm_pdata *auxpcm_pdata =
			(struct msm_dai_auxpcm_pdata *) dai->dev->platform_data;

	if (params_channels(params) != 1) {
		dev_err(dai->dev, "AUX PCM supports only mono stream\n");
		return -EINVAL;
	}
	dai_data->channels = params_channels(params);

	if (params_rate(params) != 8000) {
		dev_err(dai->dev, "AUX PCM supports only 8KHz sampling rate\n");
		return -EINVAL;
	}
	dai_data->rate = params_rate(params);

	dai_data->port_config.pcm.pcm_cfg_minor_version =
				AFE_API_VERSION_PCM_CONFIG;
	dai_data->port_config.pcm.aux_mode = auxpcm_pdata->mode;
	dai_data->port_config.pcm.sync_src = auxpcm_pdata->sync;
	dai_data->port_config.pcm.frame_setting = auxpcm_pdata->frame;
	dai_data->port_config.pcm.quantype = auxpcm_pdata->quant;
	dai_data->port_config.pcm.ctrl_data_out_enable = auxpcm_pdata->data;
	dai_data->port_config.pcm.sample_rate = dai_data->rate;
	dai_data->port_config.pcm.num_channels = dai_data->channels;
	dai_data->port_config.pcm.bit_width = 16;
	dai_data->port_config.pcm.slot_number_mapping[0] = auxpcm_pdata->slot;

	return 0;
}

static void msm_dai_q6_auxpcm_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	int rc = 0;

	mutex_lock(&aux_pcm_mutex);

	if (aux_pcm_count == 0) {
		dev_dbg(dai->dev, "%s(): dai->id %d aux_pcm_count is 0. Just return\n",
				__func__, dai->id);
		mutex_unlock(&aux_pcm_mutex);
		return;
	}

	aux_pcm_count--;

	if (aux_pcm_count > 0) {
		dev_dbg(dai->dev, "%s(): dai->id %d aux_pcm_count = %d\n",
			__func__, dai->id, aux_pcm_count);
		mutex_unlock(&aux_pcm_mutex);
		return;
	} else if (aux_pcm_count < 0) {
		dev_err(dai->dev, "%s(): ERROR: dai->id %d aux_pcm_count = %d < 0\n",
			__func__, dai->id, aux_pcm_count);
		aux_pcm_count = 0;
		mutex_unlock(&aux_pcm_mutex);
		return;
	}

	pr_debug("%s: dai->id = %d aux_pcm_count = %d\n", __func__,
			dai->id, aux_pcm_count);

	rc = afe_close(PCM_RX); /* can block */
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close PCM_RX  AFE port\n");

	rc = afe_close(PCM_TX);
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close AUX PCM TX port\n");

	mutex_unlock(&aux_pcm_mutex);
}

static int msm_dai_q6_auxpcm_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	mutex_lock(&aux_pcm_mutex);

	if (aux_pcm_count == 2) {
		dev_dbg(dai->dev, "%s(): dai->id %d aux_pcm_count is 2. Just return.\n",
			__func__, dai->id);
		mutex_unlock(&aux_pcm_mutex);
		return 0;
	} else if (aux_pcm_count > 2) {
		dev_err(dai->dev, "%s(): ERROR: dai->id %d aux_pcm_count = %d > 2\n",
			__func__, dai->id, aux_pcm_count);
		mutex_unlock(&aux_pcm_mutex);
		return 0;
	}

	aux_pcm_count++;
	if (aux_pcm_count == 2)  {
		dev_dbg(dai->dev, "%s(): dai->id %d aux_pcm_count = %d after increment\n",
				__func__, dai->id, aux_pcm_count);
		mutex_unlock(&aux_pcm_mutex);
		return 0;
	}

	pr_debug("%s:dai->id:%d  aux_pcm_count = %d. opening afe\n",
			__func__, dai->id, aux_pcm_count);

	rc = afe_q6_interface_prepare();
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to open AFE APR\n");

	/*
	 * For AUX PCM Interface the below sequence of clk
	 * settings and afe_open is a strict requirement.
	 *
	 * Also using afe_open instead of afe_port_start_nowait
	 * to make sure the port is open before deasserting the
	 * clock line. This is required because pcm register is
	 * not written before clock deassert. Hence the hw does
	 * not get updated with new setting if the below clock
	 * assert/deasset and afe_open sequence is not followed.
	 */

	afe_open(PCM_RX, &dai_data->port_config, dai_data->rate);

	afe_open(PCM_TX, &dai_data->port_config, dai_data->rate);

	mutex_unlock(&aux_pcm_mutex);

	return rc;
}

static int msm_dai_q6_auxpcm_trigger(struct snd_pcm_substream *substream,
		int cmd, struct snd_soc_dai *dai)
{
	int rc = 0;

	pr_debug("%s:port:%d  cmd:%d  aux_pcm_count= %d",
		__func__, dai->id, cmd, aux_pcm_count);

	switch (cmd) {

	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* afe_open will be called from prepare */
		return 0;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		return 0;

	default:
		rc = -EINVAL;
	}

	return rc;

}

static int msm_dai_q6_dai_auxpcm_probe(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data;
	int rc = 0;
	struct msm_dai_auxpcm_pdata *auxpcm_pdata = NULL;

	auxpcm_pdata = (struct msm_dai_auxpcm_pdata *)
					dev_get_drvdata(dai->dev);
	dai->dev->platform_data = auxpcm_pdata;

	mutex_lock(&aux_pcm_mutex);

	/*
	 * The clk name for AUX PCM operation is passed as platform
	 * data to the cpu driver, since cpu drive is unaware of any
	 * boarc specific configuration.
	 */
	if (!pcm_clk)
		pcm_clk = clk_get(dai->dev, auxpcm_pdata->clk);

	mutex_unlock(&aux_pcm_mutex);

	dai_data = kzalloc(sizeof(struct msm_dai_q6_dai_data), GFP_KERNEL);

	if (!dai_data) {
		dev_err(dai->dev, "DAI-%d: fail to allocate dai data\n",
		dai->id);
		rc = -ENOMEM;
	} else
		dev_set_drvdata(dai->dev, dai_data);

	pr_err("%s : probe done for dai->id %d\n", __func__, dai->id);
	return rc;
}

static int msm_dai_q6_dai_auxpcm_remove(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data;
	int rc;

	dai_data = dev_get_drvdata(dai->dev);

	mutex_lock(&aux_pcm_mutex);

	if (aux_pcm_count == 0) {
		dev_dbg(dai->dev, "%s(): dai->id %d aux_pcm_count is 0. clean up and return\n",
					__func__, dai->id);
		goto done;
	}

	aux_pcm_count--;

	if (aux_pcm_count > 0) {
		dev_dbg(dai->dev, "%s(): dai->id %d aux_pcm_count = %d\n",
			__func__, dai->id, aux_pcm_count);
		goto done;
	} else if (aux_pcm_count < 0) {
		dev_err(dai->dev, "%s(): ERROR: dai->id %d aux_pcm_count = %d < 0\n",
			__func__, dai->id, aux_pcm_count);
		goto done;
	}

	dev_dbg(dai->dev, "%s(): dai->id %d aux_pcm_count = %d.closing afe\n",
		__func__, dai->id, aux_pcm_count);

	rc = afe_close(PCM_RX); /* can block */
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close AUX PCM RX AFE port\n");

	rc = afe_close(PCM_TX);
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close AUX PCM TX AFE port\n");

done:
	kfree(dai_data);
	snd_soc_unregister_dai(dai->dev);

	mutex_unlock(&aux_pcm_mutex);

	return 0;
}

static struct snd_soc_dai_ops msm_dai_q6_auxpcm_ops = {
	.prepare	= msm_dai_q6_auxpcm_prepare,
	.trigger	= msm_dai_q6_auxpcm_trigger,
	.hw_params	= msm_dai_q6_auxpcm_hw_params,
	.shutdown	= msm_dai_q6_auxpcm_shutdown,
};

static struct snd_soc_dai_driver msm_dai_q6_aux_pcm_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_8000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 1,
		.rate_max = 8000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_auxpcm_ops,
	.probe = msm_dai_q6_dai_auxpcm_probe,
	.remove = msm_dai_q6_dai_auxpcm_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_aux_pcm_tx_dai = {
	.capture = {
		.rates = SNDRV_PCM_RATE_8000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 1,
		.rate_max = 8000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_auxpcm_ops,
	.probe = msm_dai_q6_dai_auxpcm_probe,
	.remove = msm_dai_q6_dai_auxpcm_remove,
};

static int msm_auxpcm_dev_probe(struct platform_device *pdev)
{
	int id;
	void *plat_data;
	int rc = 0;

	if (pdev->dev.parent == NULL)
		return -ENODEV;

	plat_data = dev_get_drvdata(pdev->dev.parent);

	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,msm-auxpcm-dev-id", &id);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-auxpcm-dev-id missing in DT node\n",
				__func__);
		return rc;
	}

	pdev->id = id;
	dev_set_name(&pdev->dev, "%s.%d", "msm-dai-q6", id);
	dev_dbg(&pdev->dev, "dev name %s\n", dev_name(&pdev->dev));

	dev_set_drvdata(&pdev->dev, plat_data);

	switch (id) {
	case AFE_PORT_ID_PRIMARY_PCM_RX:
		rc = snd_soc_register_dai(&pdev->dev,
					&msm_dai_q6_aux_pcm_rx_dai);
		break;
	case AFE_PORT_ID_PRIMARY_PCM_TX:
		rc = snd_soc_register_dai(&pdev->dev,
					&msm_dai_q6_aux_pcm_tx_dai);
		break;
	default:
		rc = -ENODEV;
		break;
	}

	return rc;
}

static int msm_auxpcm_resource_probe(
			struct platform_device *pdev)
{
	int rc = 0;
	struct msm_dai_auxpcm_pdata *auxpcm_pdata = NULL;
	u32 property_val;

	auxpcm_pdata = kzalloc(sizeof(struct msm_dai_auxpcm_pdata),
				GFP_KERNEL);

	if (!auxpcm_pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory for platform data\n");
		return -ENOMEM;
	}

	rc = of_property_read_string(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-clk",
			&auxpcm_pdata->clk);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-clk missing in DT node\n",
			__func__);
		goto fail_free_plat;
	}
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-mode", &property_val);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-mode missing in DT node\n",
			__func__);
		goto fail_free_plat;
	}
	auxpcm_pdata->mode = (u16)property_val;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-sync", &property_val);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-sync missing in DT node\n",
			__func__);
		goto fail_free_plat;
	}
	auxpcm_pdata->sync = (u16)property_val;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-frame", &property_val);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-frame missing in DT node\n",
			__func__);
		goto fail_free_plat;
	}
	auxpcm_pdata->frame = (u16)property_val;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-quant", &property_val);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-quant missing in DT node\n",
			__func__);
		goto fail_free_plat;
	}
	auxpcm_pdata->quant = (u16)property_val;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-slot", &property_val);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-slot missing in DT node\n",
			__func__);
		goto fail_free_plat;
	}
	auxpcm_pdata->slot = (u16)property_val;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-data", &property_val);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-data missing in DT node\n",
			__func__);
		goto fail_free_plat;
	}
	auxpcm_pdata->data = (u16)property_val;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-pcm-clk-rate",
			&auxpcm_pdata->pcm_clk_rate);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-pcm-clk-rate missing in DT node\n",
			__func__);
		goto fail_free_plat;
	}
	platform_set_drvdata(pdev, auxpcm_pdata);

	rc = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (rc) {
		dev_err(&pdev->dev, "%s: failed to add child nodes, rc=%d\n",
				__func__, rc);
		goto fail_free_plat;
	}

	return rc;

fail_free_plat:
	kfree(auxpcm_pdata);
	return rc;
}

static int msm_auxpcm_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static int msm_auxpcm_resource_remove(
				struct platform_device *pdev)
{
	void *auxpcm_pdata;

	auxpcm_pdata = dev_get_drvdata(&pdev->dev);
	kfree(auxpcm_pdata);

	return 0;
}

static const struct of_device_id msm_auxpcm_resource_dt_match[] = {
	{ .compatible = "qcom,msm-auxpcm-resource", },
	{}
};
MODULE_DEVICE_TABLE(of, msm_auxpcm_resource_dt_match);

static const struct of_device_id msm_auxpcm_dev_dt_match[] = {
	{ .compatible = "qcom,msm-auxpcm-dev", },
	{}
};
MODULE_DEVICE_TABLE(of, msm_auxpcm_dev_dt_match);


static struct platform_driver msm_auxpcm_dev = {
	.probe  = msm_auxpcm_dev_probe,
	.remove = msm_auxpcm_dev_remove,
	.driver = {
		.name = "msm-auxpcm-dev",
		.owner = THIS_MODULE,
		.of_match_table = msm_auxpcm_dev_dt_match,
	},
};

static struct platform_driver msm_auxpcm_resource = {
	.probe  = msm_auxpcm_resource_probe,
	.remove  = msm_auxpcm_resource_remove,
	.driver = {
		.name = "msm-auxpcm-resource",
		.owner = THIS_MODULE,
		.of_match_table = msm_auxpcm_resource_dt_match,
	},
};


static int __init msm_dai_q6_init(void)
{
	int rc;

	rc = platform_driver_register(&msm_auxpcm_dev);
	if (rc)
		goto fail;

	rc = platform_driver_register(&msm_auxpcm_resource);

	if (rc) {
		pr_err("%s: fail to register cpu dai driver\n", __func__);
		platform_driver_unregister(&msm_auxpcm_dev);
	}
fail:
	return rc;
}
module_init(msm_dai_q6_init);

static void __exit msm_dai_q6_exit(void)
{
	platform_driver_unregister(&msm_auxpcm_dev);
	platform_driver_unregister(&msm_auxpcm_resource);
}
module_exit(msm_dai_q6_exit);

/* Module information */
MODULE_DESCRIPTION("MSM DSP DAI driver");
MODULE_LICENSE("GPL v2");
