/*
 * Copyright (c) 2013-2014, Linux Foundation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <sound/soc.h>
#include <sound/cpe_core.h>
#include <sound/lsm_params.h>

#define LSM_VOICE_WAKEUP_APP_V2 2

enum {
	AFE_CMD_INVALID = 0,
	AFE_CMD_PORT_START,
	AFE_CMD_PORT_SUSPEND,
	AFE_CMD_PORT_RESUME,
	AFE_CMD_PORT_STOP,
};

struct cpe_priv {
	void *core_handle;
	struct snd_soc_codec *codec;
	struct wcd_cpe_lsm_ops lsm_ops;
	struct wcd_cpe_afe_ops afe_ops;
};

struct cpe_lsm_data {
	struct device *dev;
	struct cpe_lsm_session *lsm_session;

	wait_queue_head_t event_wait;
	atomic_t event_avail;
	atomic_t event_stop;

	u8 ev_det_status;
	u8 ev_det_pld_size;
	u8 *ev_det_payload;
};

/*
 * cpe_get_private_data: obtain ASoC platform driver private data
 * @substream: ASoC substream for which private data to be obtained
 */
static struct cpe_priv *cpe_get_private_data(
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_platform *platform = rtd->platform;

	return snd_soc_platform_get_drvdata(platform);
}

/*
 * cpe_get_lsm_data: obtain the lsm session data given the substream
 * @substream: ASoC substream for which lsm session data to be obtained
 */
static struct cpe_lsm_data *cpe_get_lsm_data(
	struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	return runtime->private_data;
}

static void msm_cpe_process_event_status(void *lsm_data,
		u8 detect_status, u8 size, u8 *payload)
{
	struct cpe_lsm_data *lsm_d = lsm_data;

	lsm_d->ev_det_status = detect_status;
	lsm_d->ev_det_pld_size = size;

	lsm_d->ev_det_payload = kzalloc(size, GFP_KERNEL);
	if (!lsm_d->ev_det_payload) {
		pr_err("%s: no memory for event payload, size = %u\n",
			__func__, size);
		return;
	}
	memcpy(lsm_d->ev_det_payload, payload, size);

	atomic_set(&lsm_d->event_avail, 1);
	wake_up(&lsm_d->event_wait);
}

static void msm_cpe_process_event_status_done(struct cpe_lsm_data *lsm_data)
{
	kfree(lsm_data->ev_det_payload);
	lsm_data->ev_det_status = 0;
	lsm_data->ev_det_pld_size = 0;
}

/*
 * msm_cpe_afe_port_cntl: Perform the afe port control
 * @substream: substream for which afe port command to be performed
 * @core_handle: handle to core
 * @afe_ops: handle to the afe operations
 * @afe_cfg: afe port configuration data
 * @cmd: command to be sent to AFE
 *
 */
static int msm_cpe_afe_port_cntl(
		struct snd_pcm_substream *substream,
		void *core_handle,
		struct wcd_cpe_afe_ops *afe_ops,
		struct wcd_cpe_afe_port_cfg *afe_cfg,
		int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int rc = 0;

	if (!afe_cfg->port_id) {
		dev_err(rtd->dev,
			"%s: Invalid afe port id\n",
			__func__);
		return -EINVAL;
	}

	switch (cmd) {
	case AFE_CMD_PORT_START:
		rc = afe_ops->afe_port_start(core_handle, afe_cfg);
		if (rc != 0)
			dev_err(rtd->dev,
				"%s: AFE port start failed\n",
				__func__);
		break;
	case AFE_CMD_PORT_SUSPEND:
		rc = afe_ops->afe_port_suspend(core_handle, afe_cfg);
		if (rc != 0)
			dev_err(rtd->dev,
				"%s: afe_suspend failed, err = %d\n",
				__func__, rc);
		break;
	case AFE_CMD_PORT_RESUME:
		rc = afe_ops->afe_port_resume(core_handle, afe_cfg);
		if (rc != 0)
			dev_err(rtd->dev,
				"%s: afe_resume failed, err = %d\n",
				__func__, rc);
		break;
	case AFE_CMD_PORT_STOP:
		rc = afe_ops->afe_port_stop(core_handle, afe_cfg);
		if (rc != 0)
			dev_err(rtd->dev,
				"%s: afe_stopfailed, err = %d\n",
				__func__, rc);
		break;
	}

	return rc;
}

/*
 * msm_cpe_lsm_open: ASoC call to open the stream
 * @substream: substream that is to be opened
 *
 * Create session data for lsm session and open the lsm session
 * on CPE.
 */
static int msm_cpe_lsm_open(struct snd_pcm_substream *substream)
{
	struct cpe_lsm_data *lsm_d;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct cpe_priv *cpe = cpe_get_private_data(substream);
	struct wcd_cpe_lsm_ops *lsm_ops;
	int rc = 0;

	if (!cpe || !cpe->codec) {
		dev_err(rtd->dev,
			"%s: Invalid private data\n",
			__func__);
		return -EINVAL;
	}

	cpe->core_handle = wcd_cpe_get_core_handle(cpe->codec);

	if (!cpe->core_handle) {
		dev_err(rtd->dev,
			"%s: Invalid handle to codec core\n",
			__func__);
		return -EINVAL;
	}

	lsm_ops = &cpe->lsm_ops;
	lsm_d = kzalloc(sizeof(struct cpe_lsm_data), GFP_KERNEL);
	if (!lsm_d) {
		dev_err(rtd->dev,
			"%s: ENOMEM for lsm session, size = %zd\n",
			__func__, sizeof(struct cpe_lsm_data));
		rc = -ENOMEM;
		goto fail_return;
	}

	lsm_d->lsm_session = lsm_ops->lsm_alloc_session(cpe->core_handle,
					lsm_d, msm_cpe_process_event_status);

	if (!lsm_d->lsm_session) {
		dev_err(rtd->dev,
			"%s: session allocation failed",
			__func__);
		rc = -EINVAL;
		goto fail_session_alloc;
	}

	dev_dbg(rtd->dev, "%s: allocated session with id = %d\n",
		__func__, lsm_d->lsm_session->id);


	rc = lsm_ops->lsm_open_tx(cpe->core_handle, lsm_d->lsm_session,
				   LSM_VOICE_WAKEUP_APP_V2, 16000);
	if (rc  < 0) {
		dev_err(rtd->dev,
			"%s: OPEN_TX cmd failed, err = %d\n",
			__func__, rc);
		goto fail_open_tx;
	}

	init_waitqueue_head(&lsm_d->event_wait);
	atomic_set(&lsm_d->event_avail, 0);
	atomic_set(&lsm_d->event_stop, 0);
	runtime->private_data = lsm_d;

	return 0;

fail_open_tx:
	lsm_ops->lsm_dealloc_session(cpe->core_handle, lsm_d->lsm_session);

fail_session_alloc:
	kfree(lsm_d);
fail_return:
	return rc;
}

/*
 * msm_cpe_lsm_close: ASoC call to close/cleanup the stream
 * @substream: substream that is to be closed
 *
 * Deallocate the session and release the AFE port. It is not
 * required to deregister the sound model as long as we close
 * the lsm session on CPE.
 */
static int msm_cpe_lsm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct cpe_lsm_data *lsm_d = cpe_get_lsm_data(substream);
	struct cpe_priv *cpe = cpe_get_private_data(substream);
	struct wcd_cpe_lsm_ops *lsm_ops;
	struct cpe_lsm_session *session;
	struct wcd_cpe_afe_ops *afe_ops;
	struct wcd_cpe_afe_port_cfg *afe_cfg;
	int rc = 0;

	if (!cpe || !cpe->core_handle) {
		dev_err(rtd->dev,
			"%s: Invalid private data\n",
			__func__);
		return -EINVAL;
	}

	if (!lsm_d || !lsm_d->lsm_session) {
		dev_err(rtd->dev,
			"%s: Invalid session data\n",
			__func__);
		return -EINVAL;
	}

	lsm_ops = &cpe->lsm_ops;
	session = lsm_d->lsm_session;
	afe_ops = &cpe->afe_ops;
	afe_cfg = &(lsm_d->lsm_session->afe_port_cfg);

	rc = msm_cpe_afe_port_cntl(substream,
				   cpe->core_handle,
				   afe_ops, afe_cfg,
				   AFE_CMD_PORT_STOP);

	rc = lsm_ops->lsm_close_tx(cpe->core_handle, session);
	if (rc != 0) {
		dev_err(rtd->dev,
			"%s: lsm_close fail, err = %d\n",
			__func__, rc);
		return rc;
	}
	lsm_ops->lsm_dealloc_session(cpe->core_handle, session);
	runtime->private_data = NULL;
	kfree(lsm_d);

	return rc;
}

/*
 * msm_cpe_lsm_ioctl: IOCTL for this platform driver
 * @substream: ASoC substream for which the operation is invoked
 * @cmd: command for the ioctl
 * @arg: argument for the ioctl
 *
 * Perform dedicated listen functions like register sound model,
 * deregister sound model, etc
 */
static int msm_cpe_lsm_ioctl(struct snd_pcm_substream *substream,
			 unsigned int cmd, void *arg)
{
	struct snd_lsm_sound_model_v2 snd_model;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct cpe_lsm_data *lsm_d = cpe_get_lsm_data(substream);
	struct cpe_priv *cpe = cpe_get_private_data(substream);
	struct cpe_lsm_session *session;
	struct wcd_cpe_lsm_ops *lsm_ops;
	struct snd_lsm_event_status *event_status = NULL;
	struct snd_lsm_event_status u_event_status;
	int rc = 0, u_pld_size = 0;

	if (!cpe || !cpe->core_handle) {
		dev_err(rtd->dev,
			"%s: Invalid private data\n",
			__func__);
		return -EINVAL;
	}

	if (!lsm_d || !lsm_d->lsm_session) {
		dev_err(rtd->dev,
			"%s: Invalid session data\n",
			__func__);
		return -EINVAL;
	}

	session = lsm_d->lsm_session;
	lsm_ops = &cpe->lsm_ops;

	dev_dbg(rtd->dev, "%s: cmd = %u\n",
		__func__, cmd);

	switch (cmd) {
	case SNDRV_LSM_REG_SND_MODEL_V2:

		if (copy_from_user(&snd_model, (void *)arg,
				   sizeof(struct snd_lsm_sound_model_v2))) {
			dev_err(rtd->dev,
				"%s: copy from user failed, size %zd\n",
				__func__,
				sizeof(struct snd_lsm_sound_model_v2));
			return -EFAULT;
		}

		if (snd_model.num_confidence_levels <= 0) {
			dev_err(rtd->dev,
				"%s: Invalid number of confidence levels %u\n",
				__func__, snd_model.num_confidence_levels);
			return -EINVAL;
		}

		session->conf_levels = kzalloc(snd_model.num_confidence_levels,
					       GFP_KERNEL);
		if (!session->conf_levels) {
			dev_err(rtd->dev,
				"%s: No memory for confidence levels %u\n",
				__func__, snd_model.num_confidence_levels);
			return -ENOMEM;
		}

		if (copy_from_user(session->conf_levels,
				  snd_model.confidence_level,
				  snd_model.num_confidence_levels)) {
			dev_err(rtd->dev,
				"%s: copy_from_user failed for confidence levels %u\n",
				__func__, snd_model.num_confidence_levels);
			kfree(session->conf_levels);
			return -EFAULT;
		}
		session->num_confidence_levels =
				snd_model.num_confidence_levels;

		session->snd_model_data = kzalloc(snd_model.data_size,
						  GFP_KERNEL);
		if (!session->snd_model_data) {
			dev_err(rtd->dev, "%s: No memory for sound model\n",
				__func__);
			kfree(session->conf_levels);
			return -ENOMEM;
		}
		session->snd_model_size = snd_model.data_size;

		if (copy_from_user(session->snd_model_data,
				   snd_model.data, snd_model.data_size)) {
			dev_err(rtd->dev,
				"%s: copy_from_user failed for snd_model\n",
				__func__);
			kfree(session->conf_levels);
			kfree(session->snd_model_data);
			return -EFAULT;
		}

		rc = lsm_ops->lsm_shmem_alloc(cpe->core_handle, session,
					       session->snd_model_size);
		if (rc != 0) {
			dev_err(rtd->dev,
				"%s: shared memory allocation failed, err = %d\n",
			       __func__, rc);
			kfree(session->snd_model_data);
			kfree(session->conf_levels);
			return rc;
		}

		rc = lsm_ops->lsm_register_snd_model(cpe->core_handle, session,
						snd_model.detection_mode,
						snd_model.detect_failure);
		if (rc != 0) {
			dev_err(rtd->dev,
				"%s: snd_model_reg failed, err = %d\n",
			       __func__, rc);
			lsm_ops->lsm_shmem_dealloc(cpe->core_handle, session);
			kfree(session->snd_model_data);
			kfree(session->conf_levels);
			return rc;
		}

		break;

	case SNDRV_LSM_DEREG_SND_MODEL:
		rc = lsm_ops->lsm_deregister_snd_model(
				cpe->core_handle, session);
		if (rc != 0) {
			dev_err(rtd->dev,
				"%s: snd_model de-reg failed, err = %d\n",
				__func__, rc);
			return rc;
		}

		kfree(session->snd_model_data);
		kfree(session->conf_levels);

		rc = lsm_ops->lsm_shmem_dealloc(cpe->core_handle, session);
		if (rc != 0) {
			dev_err(rtd->dev,
				"%s: LSM shared memory dealloc failed, err = %d\n",
				__func__, rc);
			return rc;
		}

		break;

	case SNDRV_LSM_EVENT_STATUS:

		if (copy_from_user(&u_event_status, (void *)arg,
				   sizeof(struct snd_lsm_event_status))) {
			dev_err(rtd->dev,
				"%s: event status copy from user failed, size %zd\n",
				__func__,
				sizeof(struct snd_lsm_event_status));
			return -EFAULT;
		}
		u_pld_size = sizeof(struct snd_lsm_event_status) +
				u_event_status.payload_size;

		event_status = kzalloc(u_pld_size, GFP_KERNEL);
		if (!event_status) {
			dev_err(rtd->dev,
				"%s: No memory for event status\n",
				__func__);
			return -ENOMEM;
		}

		rc = wait_event_interruptible(lsm_d->event_wait,
				(atomic_read(&lsm_d->event_avail) == 1) ||
				(atomic_read(&lsm_d->event_stop) == 1));

		if (!rc) {
			if (atomic_read(&lsm_d->event_avail) == 1) {
				rc = 0;
				atomic_set(&lsm_d->event_avail, 0);
				if (lsm_d->ev_det_pld_size >
					u_event_status.payload_size) {
					dev_err(rtd->dev,
						"%s: avail pld_bytes = %u, needed = %u\n",
						__func__,
						u_event_status.payload_size,
						lsm_d->ev_det_pld_size);
					kfree(event_status);
					return -EINVAL;
				}

				event_status->status = lsm_d->ev_det_status;
				event_status->payload_size =
						lsm_d->ev_det_pld_size;
				memcpy(event_status->payload,
				       lsm_d->ev_det_payload,
				       lsm_d->ev_det_pld_size);

				if (copy_to_user(arg, event_status,
						u_pld_size)) {
					dev_err(rtd->dev,
						"%s: copy to user failed\n",
						__func__);
					kfree(event_status);
					return -EFAULT;
				}
				msm_cpe_process_event_status_done(lsm_d);
			} else if (atomic_read(&lsm_d->event_stop) == 1) {
				dev_dbg(rtd->dev,
					"%s: wait_aborted\n", __func__);
				rc = 0;
			}
		}

		kfree(event_status);
		break;

	case SNDRV_LSM_ABORT_EVENT:
		atomic_set(&lsm_d->event_stop, 1);
		wake_up(&lsm_d->event_wait);
		break;

	case SNDRV_LSM_START:
		rc = lsm_ops->lsm_start(cpe->core_handle, session);
		if (rc != 0) {
			dev_err(rtd->dev,
				"%s: lsm_start fail, err = %d\n",
				__func__, rc);
			return rc;
		}
		break;

	case SNDRV_LSM_STOP:
		rc = lsm_ops->lsm_stop(cpe->core_handle, session);
		if (rc != 0) {
			dev_err(rtd->dev,
				"%s: lsm_stop fail err = %d\n",
				__func__, rc);

			return rc;
		}
		break;

	default:
		dev_dbg(rtd->dev, "%s: Default snd_lib_ioctl cmd 0x%x\n",
		       __func__, cmd);
		rc = snd_pcm_lib_ioctl(substream, cmd, arg);
	}

	return rc;
}

/*
 * msm_cpe_lsm_prepare: prepare call from ASoC core for this platform
 * @substream: ASoC substream for which the operation is invoked
 *
 * start the AFE port on CPE associated for this listen session
 */
static int msm_cpe_lsm_prepare(struct snd_pcm_substream *substream)
{
	int rc = 0;
	struct cpe_priv *cpe = cpe_get_private_data(substream);
	struct cpe_lsm_data *lsm_d = cpe_get_lsm_data(substream);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct wcd_cpe_afe_ops *afe_ops;
	struct wcd_cpe_afe_port_cfg *afe_cfg;

	if (!cpe || !cpe->core_handle) {
		dev_err(rtd->dev,
			"%s: Invalid private data\n",
			__func__);
		return -EINVAL;
	}

	if (!lsm_d || !lsm_d->lsm_session) {
		dev_err(rtd->dev,
			"%s: Invalid session data\n",
			__func__);
		return -EINVAL;
	}

	afe_ops = &cpe->afe_ops;
	afe_cfg = &(lsm_d->lsm_session->afe_port_cfg);

	afe_cfg->port_id = 1;
	afe_cfg->bit_width = 16;
	afe_cfg->num_channels = 1;
	afe_cfg->sample_rate = 16000;

	rc = afe_ops->afe_set_params(cpe->core_handle,
				     afe_cfg);
	if (rc != 0) {
		dev_err(rtd->dev,
			"%s: cpe afe params failed, err = %d\n",
			 __func__, rc);
		return rc;
	}

	rc = msm_cpe_afe_port_cntl(substream,
				   cpe->core_handle,
				   afe_ops, afe_cfg,
				   AFE_CMD_PORT_START);

	return rc;
}

/*
 * msm_cpe_lsm_trigger: trigger call from ASoC core for this platform
 * @substream: ASoC substream for which the operation is invoked
 * @cmd: the trigger command from framework
 *
 * suspend/resume the AFE port on CPE associated with listen session
 */
static int msm_cpe_lsm_trigger(struct snd_pcm_substream *substream,
			       int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct cpe_priv *cpe = cpe_get_private_data(substream);
	struct cpe_lsm_data *lsm_d = cpe_get_lsm_data(substream);
	struct wcd_cpe_afe_ops *afe_ops;
	struct wcd_cpe_afe_port_cfg *afe_cfg;
	int afe_cmd = AFE_CMD_INVALID;
	int rc = 0;

	if (!cpe || !cpe->core_handle) {
		dev_err(rtd->dev,
			"%s: Invalid private data\n",
			__func__);
		return -EINVAL;
	}

	if (!lsm_d || !lsm_d->lsm_session) {
		dev_err(rtd->dev,
			"%s: Invalid session data\n",
			__func__);
		return -EINVAL;
	}

	afe_ops = &cpe->afe_ops;
	afe_cfg = &(lsm_d->lsm_session->afe_port_cfg);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		afe_cmd = AFE_CMD_PORT_SUSPEND;
		break;

	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		afe_cmd = AFE_CMD_PORT_RESUME;
		break;

	default:
		afe_cmd = AFE_CMD_INVALID;
		dev_dbg(rtd->dev,
			"%s: unhandled trigger cmd %d\n",
			__func__, cmd);
		break;
	}

	if (afe_cmd != AFE_CMD_INVALID)
		rc = msm_cpe_afe_port_cntl(substream,
					   cpe->core_handle,
					   afe_ops, afe_cfg,
					   afe_cmd);

	return rc;
}

/*
 * msm_asoc_cpe_lsm_probe: ASoC framework for lsm platform driver
 * @platform: platform registered with ASoC core
 *
 * Allocate the private data for this platform and obtain the ops for
 * lsm and afe modules from underlying driver. Also find the codec
 * for this platform as specified by machine driver for ASoC framework.
 */
static int msm_asoc_cpe_lsm_probe(struct snd_soc_platform *platform)
{
	struct snd_soc_card *card;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_codec *codec;
	struct cpe_priv *cpe_priv;
	bool found_runtime = false;
	int i;

	if (!platform || !platform->card) {
		pr_err("%s: Invalid platform or card\n",
			__func__);
		return -EINVAL;
	}

	card = platform->card;

	/* Match platform to codec */
	for (i = 0; i < card->num_links; i++) {
		rtd = &card->rtd[i];
		if (!rtd->platform)
			continue;
		if (!strcmp(rtd->platform->name, platform->name)) {
			found_runtime = true;
			break;
		}
	}

	if (!found_runtime) {
		dev_err(platform->dev,
			"%s: Failed to find runtime for platform\n",
			__func__);
		return -EINVAL;
	}

	codec = rtd->codec;

	cpe_priv = kzalloc(sizeof(struct cpe_priv),
			   GFP_KERNEL);
	if (!cpe_priv) {
		dev_err(platform->dev,
			"%s: no memory for priv data, size = %zd\n",
			__func__, sizeof(struct cpe_priv));
		return -ENOMEM;
	}

	cpe_priv->codec = codec;
	wcd_cpe_get_lsm_ops(&cpe_priv->lsm_ops);
	wcd_cpe_get_afe_ops(&cpe_priv->afe_ops);

	snd_soc_platform_set_drvdata(platform, cpe_priv);
	return 0;
}

static struct snd_pcm_ops msm_cpe_lsm_ops = {
	.open = msm_cpe_lsm_open,
	.close = msm_cpe_lsm_close,
	.ioctl = msm_cpe_lsm_ioctl,
	.prepare = msm_cpe_lsm_prepare,
	.trigger = msm_cpe_lsm_trigger,
};

static struct snd_soc_platform_driver msm_soc_cpe_platform = {
	.ops = &msm_cpe_lsm_ops,
	.probe = msm_asoc_cpe_lsm_probe,
};

/*
 * msm_cpe_lsm_probe: platform driver probe
 * @pdev: platform device
 *
 * Register the ASoC platform driver with ASoC core
 */
static int msm_cpe_lsm_probe(struct platform_device *pdev)
{
	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", "msm-cpe-lsm");

	return snd_soc_register_platform(&pdev->dev,
					 &msm_soc_cpe_platform);
}

/*
 * msm_cpe_lsm_remove: platform driver remove
 * @pdev: platform device
 *
 * Deregister the ASoC platform driver
 */
static int msm_cpe_lsm_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id msm_cpe_lsm_dt_match[] = {
	{.compatible = "qcom,msm-cpe-lsm" },
	{ }
};

static struct platform_driver msm_cpe_lsm_driver = {
	.driver = {
		.name = "msm-cpe-lsm",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(msm_cpe_lsm_dt_match),
	},
	.probe = msm_cpe_lsm_probe,
	.remove = msm_cpe_lsm_remove,
};
module_platform_driver(msm_cpe_lsm_driver);

MODULE_DESCRIPTION("CPE LSM platform driver");
MODULE_DEVICE_TABLE(of, msm_cpe_lsm_dt_match);
MODULE_LICENSE("GPL v2");
