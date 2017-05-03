/*
 * Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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
#include <linux/of.h>
#include <linux/bitops.h>
#include <linux/slimbus/slimbus.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/msm-slim-dma.h>

#define SLIM_DEV_NAME "msm-dai-slim"

#define SLIM_DAI_RATES (SNDRV_PCM_RATE_48000 | \
			SNDRV_PCM_RATE_8000 | \
			SNDRV_PCM_RATE_16000 | \
			SNDRV_PCM_RATE_96000 | \
			SNDRV_PCM_RATE_192000 | \
			SNDRV_PCM_RATE_384000)

#define SLIM_DAI_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
			  SNDRV_PCM_FMTBIT_S24_LE | \
			  SNDRV_PCM_FMTBIT_S32_LE)

#define DAI_STATE_INITIALIZED (0x01 << 0)
#define DAI_STATE_PREPARED (0x01 << 1)
#define DAI_STATE_RUNNING (0x01 << 2)

#define SET_DAI_STATE(status, state) \
	(status |= state)

#define CLR_DAI_STATE(status, state) \
	(status = status & (~state))

enum {
	MSM_DAI_SLIM0 = 0,
	NUM_SLIM_DAIS,
};

struct msm_slim_dai_data {
	unsigned int dai_id;
	u16 *chan_h;
	u16 *sh_ch;
	u16 grph;
	u32 rate;
	u16 bits;
	u16 ch_cnt;
	u8 status;
	struct snd_soc_dai_driver *dai_drv;
	struct msm_slim_dma_data dma_data;
	struct slim_port_cfg port_cfg;
};

struct msm_dai_slim_drv_data {
	struct slim_device *sdev;
	u16 num_dais;
	struct msm_slim_dai_data slim_dai_data[NUM_SLIM_DAIS];
};

struct msm_slim_dai_data *msm_slim_get_dai_data(
	struct msm_dai_slim_drv_data *drv_data,
	struct snd_soc_dai *dai)
{
	struct msm_slim_dai_data *dai_data_t;
	int i;

	for (i = 0; i < drv_data->num_dais; i++) {
		dai_data_t = &drv_data->slim_dai_data[i];
		if (dai_data_t->dai_id == dai->id)
			return dai_data_t;
	}

	dev_err(dai->dev,
		"%s: no dai data found for dai_id %d\n",
		__func__, dai->id);
	return NULL;
}

static int msm_dai_slim_ch_ctl(struct msm_slim_dma_data *dma_data,
	struct snd_soc_dai *dai,
	enum msm_dai_slim_event event)
{
	struct slim_device *sdev;
	struct msm_dai_slim_drv_data *drv_data;
	struct msm_slim_dai_data *dai_data;
	int rc, rc1, i;

	if (!dma_data || !dma_data->sdev) {
		pr_err("%s: Invalid %s\n", __func__,
		       (!dma_data) ? "dma_data" : "slim_device");
		return -EINVAL;
	}

	sdev = dma_data->sdev;
	drv_data = dev_get_drvdata(&sdev->dev);
	dai_data = msm_slim_get_dai_data(drv_data, dai);

	if (!dai_data) {
		dev_err(dai->dev,
			"%s: Invalid dai_data for dai_id %d\n",
			__func__, dai->id);
		return -EINVAL;
	}

	dev_dbg(&sdev->dev,
		"%s: event = 0x%x, rate = %u\n", __func__,
		event, dai_data->rate);

	switch (event) {
	case MSM_DAI_SLIM_ENABLE:

		if (!(dai_data->status & DAI_STATE_PREPARED)) {
			dev_err(&sdev->dev,
				"%s: dai id (%d) has invalid state 0x%x\n",
				__func__, dai->id, dai_data->status);
			return -EINVAL;
		}

		rc = slim_alloc_mgrports(sdev,
					 SLIM_REQ_DEFAULT, dai_data->ch_cnt,
					 &(dma_data->ph),
					 sizeof(dma_data->ph));

		if (IS_ERR_VALUE(rc)) {
			dev_err(&sdev->dev,
				"%s:alloc mgrport failed rc %d\n",
				__func__ , rc);
			goto done;
		}

		rc = slim_config_mgrports(sdev, &(dma_data->ph),
					  dai_data->ch_cnt,
					  &(dai_data->port_cfg));
		if (IS_ERR_VALUE(rc)) {
			dev_err(&sdev->dev,
				"%s: config mgrport failed rc %d\n",
				__func__ , rc);
			goto err_done;
		}

		for (i = 0; i < dai_data->ch_cnt; i++) {
			rc = slim_connect_sink(sdev,
					       &dma_data->ph, 1,
					       dai_data->chan_h[i]);
			if (IS_ERR_VALUE(rc)) {
				dev_err(&sdev->dev,
					"%s: slim_connect_sink failed, ch = %d, err = %d\n",
					__func__, i, rc);
				goto err_done;
			}
		}

		rc = slim_control_ch(sdev,
				     dai_data->grph,
				     SLIM_CH_ACTIVATE, true);
		if (IS_ERR_VALUE(rc)) {
			dev_err(&sdev->dev,
				"%s: slim activate ch failed, err = %d\n",
				__func__, rc);
			goto err_done;
		}
		/* Mark dai status as running */
		SET_DAI_STATE(dai_data->status, DAI_STATE_RUNNING);
		break;

	case MSM_DAI_SLIM_PRE_DISABLE:
		if (!(dai_data->status & DAI_STATE_RUNNING)) {
			dev_err(&sdev->dev,
				"%s: dai id (%d) has invalid state 0x%x\n",
				__func__, dai->id, dai_data->status);
			return -EINVAL;
		}

		rc = slim_control_ch(sdev,
				     dai_data->grph,
				     SLIM_CH_REMOVE, true);
		if (IS_ERR_VALUE(rc)) {
			dev_err(&sdev->dev,
				"%s: slim activate ch failed, err = %d\n",
				__func__, rc);
			goto done;
		}
		break;

	case MSM_DAI_SLIM_DISABLE:

		rc = slim_dealloc_mgrports(sdev,
					   &dma_data->ph, 1);
		if (IS_ERR_VALUE(rc)) {
			dev_err(&sdev->dev,
				"%s: dealloc mgrport failed, err = %d\n",
				__func__, rc);
			goto done;
		}
		/* clear running state for dai*/
		CLR_DAI_STATE(dai_data->status, DAI_STATE_RUNNING);
		break;

	default:
		dev_err(&sdev->dev,
			"%s: Unhandled event 0x%x\n",
			__func__, event);
		rc = -EINVAL;
		goto done;
	}

	return rc;

err_done:
	rc1 = slim_dealloc_mgrports(sdev,
				   &dma_data->ph, 1);
	if (IS_ERR_VALUE(rc1))
		dev_err(&sdev->dev,
			"%s: dealloc mgrport failed, err = %d\n",
			__func__, rc1);
done:
	return rc;
}

static int msm_dai_slim_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct msm_dai_slim_drv_data *drv_data = dev_get_drvdata(dai->dev);
	struct msm_slim_dai_data *dai_data;
	int rc = 0;

	dai_data = msm_slim_get_dai_data(drv_data, dai);
	if (!dai_data) {
		dev_err(dai->dev,
			"%s: Invalid dai_data for dai_id %d\n",
			__func__, dai->id);
		rc = -EINVAL;
		goto done;
	}

	if (!dai_data->ch_cnt || dai_data->ch_cnt != params_channels(params)) {
		dev_err(dai->dev, "%s: invalid ch_cnt %d %d\n",
			__func__, dai_data->ch_cnt, params_channels(params));
		rc = -EINVAL;
		goto done;
	}

	dai_data->rate = params_rate(params);
	dai_data->port_cfg.port_opts = SLIM_OPT_NONE;
	if (dai_data->rate >= SNDRV_PCM_RATE_48000)
		dai_data->port_cfg.watermark = 16;
	else
		dai_data->port_cfg.watermark = 8;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		dai_data->bits = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		dai_data->bits = 24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		dai_data->bits = 32;
		break;
	default:
		dev_err(dai->dev, "%s: invalid format %d\n", __func__,
			params_format(params));
		rc = -EINVAL;
		goto done;
	}

	dev_dbg(dai->dev, "%s: ch_cnt=%u rate=%u, bit_width = %u\n",
		__func__, dai_data->ch_cnt, dai_data->rate,
		dai_data->bits);
done:
	return rc;
}

static int msm_dai_slim_set_channel_map(struct snd_soc_dai *dai,
	unsigned int tx_num, unsigned int *tx_slot,
	unsigned int rx_num, unsigned int *rx_slot)
{
	struct msm_dai_slim_drv_data *drv_data = dev_get_drvdata(dai->dev);
	struct msm_slim_dai_data *dai_data;
	struct snd_soc_dai_driver *dai_drv;
	u8 i = 0;

	dev_dbg(dai->dev,
		"%s: tx_num=%u, rx_num=%u\n",
		__func__, tx_num, rx_num);

	dai_data = msm_slim_get_dai_data(drv_data, dai);
	if (!dai_data) {
		dev_err(dai->dev,
			"%s: Invalid dai_data for dai_id %d\n",
			__func__, dai->id);
		return -EINVAL;
	}

	dai_drv = dai_data->dai_drv;

	if (tx_num > dai_drv->capture.channels_max) {
		dev_err(dai->dev, "%s: tx_num %u max out master port cnt\n",
			__func__, tx_num);
		return -EINVAL;
	}

	for (i = 0; i < tx_num; i++)
		dai_data->sh_ch[i] = tx_slot[i];

	dai_data->ch_cnt = tx_num;
	return 0;
}

static int msm_dai_slim_prepare(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	struct msm_dai_slim_drv_data *drv_data = dev_get_drvdata(dai->dev);
	struct msm_slim_dma_data *dma_data;
	struct msm_slim_dai_data *dai_data = NULL;
	struct slim_ch prop;
	int rc;
	u8 i, j;

	dai_data = msm_slim_get_dai_data(drv_data, dai);
	if (!dai_data) {
		dev_err(dai->dev,
			"%s: Invalid dai_data for dai %d\n",
			__func__, dai->id);
		return -EINVAL;
	}

	if (!(dai_data->status & DAI_STATE_INITIALIZED)) {
		dev_err(dai->dev,
			"%s: dai id (%d) has invalid state 0x%x\n",
			__func__, dai->id, dai_data->status);
		return -EINVAL;
	}

	dma_data = &dai_data->dma_data;
	snd_soc_dai_set_dma_data(dai, substream, dma_data);

	for (i = 0; i < dai_data->ch_cnt; i++) {
		rc = slim_query_ch(drv_data->sdev, dai_data->sh_ch[i],
				   &dai_data->chan_h[i]);
		if (rc) {
			dev_err(dai->dev, "%s:query chan handle failed rc %d\n",
				__func__ , rc);
			goto error_chan_query;
		}
	}

	prop.prot = SLIM_AUTO_ISO;
	prop.baser = SLIM_RATE_4000HZ;
	prop.dataf = SLIM_CH_DATAF_NOT_DEFINED;
	prop.auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop.ratem = (dai_data->rate/4000);
	prop.sampleszbits = dai_data->bits;

	rc = slim_define_ch(drv_data->sdev, &prop, dai_data->chan_h,
			    dai_data->ch_cnt, true, &dai_data->grph);

	if (rc) {
		dev_err(dai->dev, "%s:define chan failed rc %d\n",
				__func__ , rc);
		goto error_define_chan;
	}

	/* Mark stream status as prepared */
	SET_DAI_STATE(dai_data->status, DAI_STATE_PREPARED);

	return rc;

error_define_chan:
error_chan_query:
	for (j = 0; j < i; j++)
		slim_dealloc_ch(drv_data->sdev, dai_data->chan_h[j]);
	return rc;
}

static void msm_dai_slim_shutdown(struct snd_pcm_substream *stream,
		struct snd_soc_dai *dai)
{
	struct msm_dai_slim_drv_data *drv_data = dev_get_drvdata(dai->dev);
	struct msm_slim_dma_data *dma_data = NULL;
	struct msm_slim_dai_data *dai_data;
	int i, rc = 0;

	dai_data = msm_slim_get_dai_data(drv_data, dai);
	dma_data = snd_soc_dai_get_dma_data(dai, stream);
	if (!dma_data || !dai_data) {
		dev_err(dai->dev,
			"%s: Invalid %s\n", __func__,
			(!dma_data) ? "dma_data" : "dai_data");
		return;
	}

	if ((!(dai_data->status & DAI_STATE_PREPARED)) ||
	     dai_data->status & DAI_STATE_RUNNING) {
		dev_err(dai->dev,
			"%s: dai id (%d) has invalid state 0x%x\n",
			__func__, dai->id, dai_data->status);
		return;
	}

	for (i = 0; i < dai_data->ch_cnt; i++) {
		rc = slim_dealloc_ch(drv_data->sdev, dai_data->chan_h[i]);
		if (rc) {
			dev_err(dai->dev,
				"%s: dealloc_ch failed, err = %d\n",
				__func__, rc);
		}
	}

	snd_soc_dai_set_dma_data(dai, stream, NULL);
	/* clear prepared state for the dai */
	CLR_DAI_STATE(dai_data->status, DAI_STATE_PREPARED);

	return;
}

static const struct snd_soc_component_driver msm_dai_slim_component = {
	.name		= "msm-dai-slim-cmpnt",
};

static struct snd_soc_dai_ops msm_dai_slim_ops = {
	.prepare	= msm_dai_slim_prepare,
	.hw_params	= msm_dai_slim_hw_params,
	.shutdown	= msm_dai_slim_shutdown,
	.set_channel_map = msm_dai_slim_set_channel_map,
};

static struct snd_soc_dai_driver msm_slim_dais[] = {
	{
		/*
		 * The first dai name should be same as device name
		 * to support registering single and multile dais.
		 */
		.name = SLIM_DEV_NAME,
		.id = MSM_DAI_SLIM0,
		.capture = {
			.rates = SLIM_DAI_RATES,
			.formats = SLIM_DAI_FORMATS,
			.channels_min = 1,
			/*
			 * max channels allowed is
			 * dependent on platform and
			 * will be updated before this
			 * dai driver is registered.
			 */
			.channels_max = 1,
			.rate_min = 8000,
			.rate_max = 384000,
			.stream_name = "SLIM_DAI0 Capture",
		},
		.ops = &msm_dai_slim_ops,
	},
	/*
	 * If multiple dais are needed,
	 * add dais here and update the
	 * dai_id enum.
	 */
};

static void msm_dai_slim_remove_dai_data(
		struct device *dev,
		struct msm_dai_slim_drv_data *drv_data)
{
	int i;
	struct msm_slim_dai_data *dai_data_t;

	for (i = 0; i < drv_data->num_dais; i++) {
		dai_data_t = &drv_data->slim_dai_data[i];

		kfree(dai_data_t->chan_h);
		dai_data_t->chan_h = NULL;
		kfree(dai_data_t->sh_ch);
		dai_data_t->sh_ch = NULL;
	}
}

static int msm_dai_slim_populate_dai_data(struct device *dev,
		struct msm_dai_slim_drv_data *drv_data)
{
	struct snd_soc_dai_driver *dai_drv;
	struct msm_slim_dai_data *dai_data_t;
	u8 num_ch;
	int i, j, rc;

	for (i = 0; i < drv_data->num_dais; i++) {
		num_ch = 0;
		dai_drv = &msm_slim_dais[i];
		num_ch += dai_drv->capture.channels_max;
		num_ch += dai_drv->playback.channels_max;

		dai_data_t = &drv_data->slim_dai_data[i];
		dai_data_t->dai_drv = dai_drv;
		dai_data_t->dai_id = dai_drv->id;
		dai_data_t->dma_data.sdev = drv_data->sdev;
		dai_data_t->dma_data.dai_channel_ctl =
				msm_dai_slim_ch_ctl;
		SET_DAI_STATE(dai_data_t->status,
			      DAI_STATE_INITIALIZED);

		dai_data_t->chan_h = devm_kzalloc(dev,
					sizeof(u16) * num_ch,
					GFP_KERNEL);
		if (!dai_data_t->chan_h) {
			dev_err(dev,
				"%s: DAI ID %d, Failed to alloc channel handles\n",
				__func__, i);
			rc = -ENOMEM;
			goto err_mem_alloc;
		}

		dai_data_t->sh_ch = devm_kzalloc(dev,
					sizeof(u16) * num_ch,
					GFP_KERNEL);
		if (!dai_data_t->sh_ch) {
			dev_err(dev,
				"%s: DAI ID %d, Failed to alloc sh_ch\n",
				__func__, i);
			rc = -ENOMEM;
			goto err_mem_alloc;
		}
	}
	return 0;

err_mem_alloc:
	for (j = 0; j < i; j++) {
		dai_data_t = &drv_data->slim_dai_data[i];

		devm_kfree(dev, dai_data_t->chan_h);
		dai_data_t->chan_h = NULL;

		devm_kfree(dev, dai_data_t->sh_ch);
		dai_data_t->sh_ch = NULL;
	}
	return rc;
}

static int msm_dai_slim_dev_probe(struct slim_device *sdev)
{
	int rc, i;
	u8 max_channels;
	u32 apps_ch_pipes;
	struct msm_dai_slim_drv_data *drv_data;
	struct device *dev = &sdev->dev;
	struct snd_soc_dai_driver *dai_drv;

	if (!dev->of_node ||
	    !dev->of_node->parent) {
		dev_err(dev,
			"%s: Invalid %s\n", __func__,
			(!dev->of_node) ? "of_node" : "parent_of_node");
		return -EINVAL;
	}

	rc = of_property_read_u32(dev->of_node->parent,
					 "qcom,apps-ch-pipes",
					 &apps_ch_pipes);
	if (rc) {
		dev_err(dev,
			"%s: Failed to lookup property %s in node %s, err = %d\n",
			__func__, "qcom,apps-ch-pipes",
			dev->of_node->parent->full_name, rc);
		goto err_ret;
	}

	max_channels = hweight_long(apps_ch_pipes);
	if (max_channels <= 0) {
		dev_err(dev,
			"%s: Invalid apps owned ports %d\n",
			__func__, max_channels);
		goto err_ret;
	}

	dev_dbg(dev, "%s: max channels = %u\n",
		__func__, max_channels);

	for (i = 0; i < ARRAY_SIZE(msm_slim_dais); i++) {
		dai_drv = &msm_slim_dais[i];
		dai_drv->capture.channels_max = max_channels;
		dai_drv->playback.channels_max = max_channels;
	}

	drv_data = devm_kzalloc(dev, sizeof(*drv_data),
				GFP_KERNEL);
	if (!drv_data) {
		dev_err(dev, "%s: dai driver struct alloc failed\n",
			__func__);
		rc = -ENOMEM;
		goto err_ret;
	}

	drv_data->sdev = sdev;
	drv_data->num_dais = NUM_SLIM_DAIS;

	rc = msm_dai_slim_populate_dai_data(dev, drv_data);
	if (rc) {
		dev_err(dev,
			"%s: failed to setup dai_data, err = %d\n",
			__func__, rc);
		goto err_populate_dai;
	}

	rc = snd_soc_register_component(&sdev->dev, &msm_dai_slim_component,
					msm_slim_dais, NUM_SLIM_DAIS);

	if (IS_ERR_VALUE(rc)) {
		dev_err(dev, "%s: failed to register DAI, err = %d\n",
			__func__, rc);
		goto err_reg_comp;
	}

	dev_set_drvdata(dev, drv_data);
	return rc;

err_reg_comp:
	msm_dai_slim_remove_dai_data(dev, drv_data);

err_populate_dai:
	devm_kfree(dev, drv_data);

err_ret:
	return rc;
}

static int msm_dai_slim_dev_remove(struct slim_device *sdev)
{
	snd_soc_unregister_component(&sdev->dev);
	return 0;
}

static const struct slim_device_id msm_dai_slim_dt_match[] = {
	{SLIM_DEV_NAME, 0 },
	{}
};

static struct slim_driver msm_dai_slim_driver = {
	.driver = {
		.name = SLIM_DEV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = msm_dai_slim_dev_probe,
	.remove = msm_dai_slim_dev_remove,
	.id_table = msm_dai_slim_dt_match,
};

static int __init msm_dai_slim_init(void)
{
	int rc;
	rc = slim_driver_register(&msm_dai_slim_driver);
	if (rc)
		pr_err("%s: failed to register with slimbus driver rc = %d",
			__func__, rc);
	return rc;
}
module_init(msm_dai_slim_init);

static void __exit msm_dai_slim_exit(void)
{
	return;
}
module_exit(msm_dai_slim_exit);

/* Module information */
MODULE_DESCRIPTION("Slimbus apps-owned channel handling driver");
MODULE_LICENSE("GPL v2");
