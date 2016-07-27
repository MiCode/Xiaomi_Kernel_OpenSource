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
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include <sound/soc.h>
#include <sound/cpe_core.h>
#include <sound/lsm_params.h>
#include <sound/pcm_params.h>


#define LSM_VOICE_WAKEUP_APP_V2 2
#define LISTEN_MIN_NUM_PERIODS     2
#define LISTEN_MAX_NUM_PERIODS     8
#define LISTEN_MAX_PERIOD_SIZE     4096
#define LISTEN_MIN_PERIOD_SIZE     320
#define LISTEN_MAX_STATUS_PAYLOAD_SIZE 256

#define MSM_CPE_LAB_THREAD_TIMEOUT (3 * (HZ/10))

#define MSM_CPE_LSM_GRAB_LOCK(lock, name)		\
{						\
	pr_debug("%s: %s lock acquire\n",	\
		 __func__, name);		\
	mutex_lock(lock);			\
}

#define MSM_CPE_LSM_REL_LOCK(lock, name)		\
{						\
	pr_debug("%s: %s lock release\n",	\
		 __func__, name);		\
	mutex_unlock(lock);			\
}

/* Conventional and unconventional sample rate supported */
static unsigned int supported_sample_rates[] = {
	8000, 16000
};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
	.mask = 0,
};


static struct snd_pcm_hardware msm_pcm_hardware_listen = {
	.info =	(SNDRV_PCM_INFO_BLOCK_TRANSFER |
		 SNDRV_PCM_INFO_MMAP_VALID |
		 SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_PAUSE |
		 SNDRV_PCM_INFO_RESUME),
	.formats = (SNDRV_PCM_FMTBIT_S16_LE),
	.rates = SNDRV_PCM_RATE_16000,
	.rate_min = 16000,
	.rate_max = 16000,
	.channels_min =	1,
	.channels_max =	1,
	.buffer_bytes_max = LISTEN_MAX_NUM_PERIODS *
			    LISTEN_MAX_PERIOD_SIZE,
	.period_bytes_min = LISTEN_MIN_PERIOD_SIZE,
	.period_bytes_max = LISTEN_MAX_PERIOD_SIZE,
	.periods_min = LISTEN_MIN_NUM_PERIODS,
	.periods_max = LISTEN_MAX_NUM_PERIODS,
	.fifo_size = 0,
};

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
	struct mutex lsm_api_lock;

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
	struct snd_soc_pcm_runtime *rtd;

	if (!substream || !substream->private_data) {
		pr_err("%s: %s is invalid\n",
			__func__,
			(!substream) ? "substream" : "private_data");
		goto err_ret;
	}

	rtd = substream->private_data;

	if (!rtd || !rtd->platform) {
		pr_err("%s: %s is invalid\n",
			 __func__,
			(!rtd) ? "runtime" : "platform");
		goto err_ret;
	}

	return snd_soc_platform_get_drvdata(rtd->platform);

err_ret:
	return NULL;
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
	lsm_data->ev_det_payload = NULL;

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

static int msm_cpe_lsm_lab_stop(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct cpe_lsm_data *lsm_d = cpe_get_lsm_data(substream);
	struct cpe_priv *cpe = cpe_get_private_data(substream);
	struct wcd_cpe_lsm_ops *lsm_ops;
	struct cpe_lsm_session *session;
	struct wcd_cpe_lsm_lab *lab_sess;
	int rc;

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
	lab_sess = &session->lab;

	if (lab_sess->thread_status != MSM_LSM_LAB_THREAD_STOP) {

		if (lab_sess->thread_status ==
		    MSM_LSM_LAB_THREAD_RUNNING) {
			dev_dbg(rtd->dev, "%s: stopping lab thread\n",
				__func__);
			rc = kthread_stop(session->lsm_lab_thread);

			/* Wait for the lab thread to exit */
			rc = wait_for_completion_timeout(
					&lab_sess->thread_complete,
					MSM_CPE_LAB_THREAD_TIMEOUT);
			if (!rc) {
				dev_err(rtd->dev,
					"%s: Wait for lab thread timedout\n",
					__func__);
				return -ETIMEDOUT;
			}
		}

		lab_sess->thread_status = MSM_LSM_LAB_THREAD_STOP;
		rc = lsm_ops->lsm_lab_stop(cpe->core_handle, session);
		if (rc) {
			dev_err(rtd->dev,
				"%s: Lab stop failed, error = %d\n",
				__func__, rc);
			return rc;
		}
	}

	return 0;
}

/*
 * msm_cpe_lab_thread: Initiated on KW detection
 * @data: lab data
 *
 * Start lab thread and call CPE core API for SLIM
 * read operations.
 */
static int msm_cpe_lab_thread(void *data)
{
	struct wcd_cpe_lsm_lab *lab = (struct wcd_cpe_lsm_lab *)data;
	struct wcd_cpe_lab_hw_params *hw_params = &lab->hw_params;
	struct wcd_cpe_core *core = (struct wcd_cpe_core *)lab->core_handle;
	struct snd_pcm_substream *substream = lab->substream;
	struct cpe_priv *cpe = cpe_get_private_data(substream);
	struct wcd_cpe_lsm_ops *lsm_ops;
	struct wcd_cpe_data_pcm_buf *cur_buf, *next_buf;
	int rc = 0;
	u32 done_len = 0;
	u32 buf_count = 1;

	allow_signal(SIGKILL);
	set_current_state(TASK_INTERRUPTIBLE);

	pr_debug("%s: Lab thread start\n", __func__);

	if (!core || !cpe) {
		pr_err("%s: Handle to %s is invalid\n",
			__func__,
			(!core) ? "core" : "cpe");
		rc = -EINVAL;
		goto done;
	}

	lsm_ops = &cpe->lsm_ops;
	memset(lab->pcm_buf[0].mem, 0, lab->pcm_size);

	if (lsm_ops->lsm_lab_data_channel_read == NULL ||
		lsm_ops->lsm_lab_data_channel_read_status == NULL) {
			pr_err("%s: slim ops not present\n", __func__);
			rc = -EINVAL;
			goto done;
	}

	if (!hw_params || !substream || !cpe) {
		pr_err("%s: Lab thread pointers NULL\n", __func__);
		rc = -EINVAL;
		goto done;
	}

	if (!kthread_should_stop()) {
		rc = lsm_ops->lsm_lab_data_channel_read(core, lab->lsm_s,
					lab->pcm_buf[0].phys,
					lab->pcm_buf[0].mem,
					hw_params->buf_sz);
		if (rc) {
			pr_err("%s:Slim read error %d\n", __func__, rc);
			goto done;
		}

		cur_buf = &lab->pcm_buf[0];
		next_buf = &lab->pcm_buf[1];
	} else {
		pr_debug("%s: LAB stopped before starting read\n",
			 __func__);
		goto done;
	}

	while (!kthread_should_stop() &&
	       lab->thread_status != MSM_LSM_LAB_THREAD_ERROR) {

		rc = lsm_ops->lsm_lab_data_channel_read(core, lab->lsm_s,
						next_buf->phys,
						next_buf->mem,
						hw_params->buf_sz);
		if (rc) {
			pr_err("%s: Thread read Slim read error %d\n",
			       __func__, rc);
			lab->thread_status = MSM_LSM_LAB_THREAD_ERROR;
		}
		rc = lsm_ops->lsm_lab_data_channel_read_status(core, lab->lsm_s,
						cur_buf->phys, &done_len);
		if (rc) {
			pr_err("%s: Wait on current buf failed %d\n",
			       __func__, rc);
			lab->thread_status = MSM_LSM_LAB_THREAD_ERROR;
		}
		if (done_len ||
		    ((!done_len) &&
		     lab->thread_status == MSM_LSM_LAB_THREAD_ERROR)) {
			atomic_inc(&lab->in_count);
			lab->dma_write += snd_pcm_lib_period_bytes(substream);
			snd_pcm_period_elapsed(substream);
			wake_up(&lab->period_wait);
			cur_buf = next_buf;
			if (buf_count >= (hw_params->period_count - 1)) {
				buf_count = 0;
				next_buf = &lab->pcm_buf[0];
			} else {
				next_buf = &lab->pcm_buf[buf_count + 1];
				buf_count++;
			}
			pr_debug("%s: Cur buf = %p Next Buf = %p\n"
				 " buf count = 0x%x\n",
				 __func__, cur_buf, next_buf, buf_count);
		} else {
			pr_err("%s: SB get status, invalid len = 0x%x\n",
				__func__, done_len);
		}
		done_len = 0;
	}

done:
	pr_debug("%s: Exiting LAB thread\n", __func__);
	complete(&lab->thread_complete);

	return 0;
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

	runtime->hw = msm_pcm_hardware_listen;

	rc = snd_pcm_hw_constraint_list(runtime, 0,
				SNDRV_PCM_HW_PARAM_RATE,
				&constraints_sample_rates);
	if (rc < 0) {
		pr_err("snd_pcm_hw_constraint_list failed rc %d\n", rc);
		return -EINVAL;
	}

	/* Ensure that buffer size is a multiple of period size */
	rc = snd_pcm_hw_constraint_integer(runtime,
					   SNDRV_PCM_HW_PARAM_PERIODS);
	if (rc < 0) {
		pr_err("%s: Unable to set pcm_param_periods, rc %d\n",
			__func__, rc);
		return -EINVAL;
	}

	rc = snd_pcm_hw_constraint_minmax(runtime,
		SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
		LISTEN_MIN_NUM_PERIODS * LISTEN_MIN_PERIOD_SIZE,
		LISTEN_MAX_NUM_PERIODS * LISTEN_MAX_PERIOD_SIZE);
	if (rc < 0) {
		pr_err("%s: Unable to set pcm constraints, rc %d\n",
			__func__, rc);
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
	mutex_init(&lsm_d->lsm_api_lock);

	lsm_d->lsm_session = lsm_ops->lsm_alloc_session(cpe->core_handle,
					lsm_d, msm_cpe_process_event_status);

	if (!lsm_d->lsm_session) {
		dev_err(rtd->dev,
			"%s: session allocation failed",
			__func__);
		rc = -EINVAL;
		goto fail_session_alloc;
	}
	/* Explicitly Assign the LAB thread to STOP state */
	lsm_d->lsm_session->lab.thread_status = MSM_LSM_LAB_THREAD_STOP;
	lsm_d->lsm_session->started = false;

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
	mutex_destroy(&lsm_d->lsm_api_lock);
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
	struct wcd_cpe_lsm_lab *lab_sess;
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
	lab_sess = &session->lab;
	afe_ops = &cpe->afe_ops;
	afe_cfg = &(lsm_d->lsm_session->afe_port_cfg);

	/*
	 * If driver is closed without stopping LAB,
	 * explicitly stop LAB before cleaning up the
	 * driver resources.
	 */
	rc = msm_cpe_lsm_lab_stop(substream);
	if (rc) {
		dev_err(rtd->dev,
			"%s: Failed to stop lab, error = %d\n",
			__func__, rc);
		return rc;
	}

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
	mutex_destroy(&lsm_d->lsm_api_lock);
	kfree(lsm_d);

	return rc;
}

static int msm_cpe_lsm_get_conf_levels(
		struct cpe_lsm_session *session,
		u8 *conf_levels_ptr)
{
	int rc = 0;

	if (session->num_confidence_levels <= 0) {
		pr_debug("%s: conf_levels (%u), skip set params\n",
			 __func__,
			session->num_confidence_levels);
		goto done;
	}

	session->conf_levels = kzalloc(session->num_confidence_levels,
				       GFP_KERNEL);
	if (!session->conf_levels) {
		pr_err("%s: No memory for confidence levels %u\n",
			__func__, session->num_confidence_levels);
		rc = -ENOMEM;
		goto done;
	}

	if (copy_from_user(session->conf_levels,
			   conf_levels_ptr,
			   session->num_confidence_levels)) {
		pr_err("%s: copy_from_user failed for confidence levels %u\n",
			__func__, session->num_confidence_levels);
		kfree(session->conf_levels);
		session->conf_levels = NULL;
		rc = -EFAULT;
		goto done;
	}

done:
	return rc;
}

/*
 * msm_cpe_lsm_ioctl_shared: Shared IOCTL for this platform driver
 * @substream: ASoC substream for which the operation is invoked
 * @cmd: command for the ioctl
 * @arg: argument for the ioctl
 *
 * Perform dedicated listen functions like register sound model,
 * deregister sound model, etc
 * Called with lsm_api_lock acquired.
 */
static int msm_cpe_lsm_ioctl_shared(struct snd_pcm_substream *substream,
			 unsigned int cmd, void *arg)
{
	struct snd_lsm_sound_model_v2 snd_model;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct cpe_lsm_data *lsm_d = cpe_get_lsm_data(substream);
	struct cpe_priv *cpe = cpe_get_private_data(substream);
	struct cpe_lsm_session *session;
	struct wcd_cpe_lsm_ops *lsm_ops;
	struct wcd_cpe_lsm_lab *lab_sess = NULL;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct snd_lsm_event_status *user;
	struct snd_lsm_detection_params det_params;
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

	session = lsm_d->lsm_session;
	lsm_ops = &cpe->lsm_ops;
	lab_sess = &session->lab;

	switch (cmd) {
	case SNDRV_LSM_STOP_LAB:
		dev_dbg(rtd->dev,
			"%s: %s, lab_enable = %d, lab_thread_ststus = %d\n",
			__func__, "SNDRV_LSM_STOP_LAB",
			lab_sess->lab_enable,
			lab_sess->thread_status);

		if (lab_sess->lab_enable &&
		    lab_sess->thread_status != MSM_LSM_LAB_THREAD_STOP) {
			atomic_inc(&lab_sess->abort_read);
			wake_up(&lab_sess->period_wait);
			rc = msm_cpe_lsm_lab_stop(substream);
			if (rc) {
				dev_err(rtd->dev,
					"%s: stop LAB failed, error = %d\n",
					__func__, rc);
				return rc;
			}
		} else if (!lab_sess->lab_enable) {
			dev_dbg(rtd->dev,
				"%s: LAB already stopped\n",
				__func__);
		}

		break;

	case SNDRV_LSM_LAB_CONTROL:
		if (copy_from_user(&lab_sess->lab_enable, (void *)arg,
				   sizeof(u32))) {
			dev_err(rtd->dev,
				"%s: copy_from_user failed, size %zd\n",
				__func__, sizeof(u32));
			return -EFAULT;
		}

		dev_dbg(rtd->dev,
			"%s: %s, lab_enable = %d\n",
			__func__, "SNDRV_LSM_LAB_CONTROL",
			lab_sess->lab_enable);

		if (lab_sess->lab_enable) {
			rc = lsm_ops->lsm_lab_control(cpe->core_handle,
					session,
					lab_sess->hw_params.buf_sz,
					lab_sess->hw_params.period_count,
					true);
			if (rc) {
				pr_err("%s: Lab Enable Failed rc %d\n",
				       __func__, rc);
				lab_sess->lab_enable = false;
				return rc;
			}

			lab_sess->substream = substream;
			dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
			dma_buf->dev.dev = substream->pcm->card->dev;
			dma_buf->private_data = NULL;
			dma_buf->area = lab_sess->pcm_buf[0].mem;
			dma_buf->addr =  lab_sess->pcm_buf[0].phys;
			dma_buf->bytes = (lab_sess->hw_params.buf_sz *
					lab_sess->hw_params.period_count);
			if (!dma_buf->area) {
				lab_sess->lab_enable = false;
				return -ENOMEM;
			}

			init_completion(&lab_sess->thread_complete);
			snd_pcm_set_runtime_buffer(substream,
						   &substream->dma_buffer);
		} else {
			/*
			 * It is possible that lab is still enabled
			 * when trying to de-allocate the lab buffer.
			 * Make sure to disable lab before de-allocating
			 * the lab buffer.
			 */
			rc = msm_cpe_lsm_lab_stop(substream);
			if (rc) {
				dev_err(rtd->dev,
					"%s: LAB stop failed, error = %d\n",
					__func__, rc);
				return rc;
			}
			rc = lsm_ops->lsm_lab_control(cpe->core_handle,
					session,
					lab_sess->hw_params.buf_sz,
					lab_sess->hw_params.period_count,
					false);
			if (rc) {
				pr_err("%s: Lab Disable Failed rc %d\n",
				       __func__, rc);
				return rc;
			}
		}
	break;
	case SNDRV_LSM_REG_SND_MODEL_V2:
		dev_dbg(rtd->dev,
			"%s: %s\n",
			__func__, "SNDRV_LSM_REG_SND_MODEL_V2");
		if (!arg) {
			dev_err(rtd->dev,
				"%s: Invalid argument to ioctl %s\n",
				__func__,
				"SNDRV_LSM_REG_SND_MODEL_V2");
			return -EINVAL;
		}

		memcpy(&snd_model, arg,
			sizeof(struct snd_lsm_sound_model_v2));

		session->num_confidence_levels =
				snd_model.num_confidence_levels;
		rc = msm_cpe_lsm_get_conf_levels(session,
				snd_model.confidence_level);
		if (rc) {
			dev_err(rtd->dev,
				"%s: %s get_conf_levels fail, err = %d\n",
				__func__, "SNDRV_LSM_REG_SND_MODEL_V2",
				rc);
			break;
		}

		session->snd_model_data = kzalloc(snd_model.data_size,
						  GFP_KERNEL);
		if (!session->snd_model_data) {
			dev_err(rtd->dev, "%s: No memory for sound model\n",
				__func__);
			kfree(session->conf_levels);
			session->conf_levels = NULL;
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
			session->conf_levels = NULL;
			session->snd_model_data = NULL;
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
			session->snd_model_data = NULL;
			session->conf_levels = NULL;
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
			session->snd_model_data = NULL;
			session->conf_levels = NULL;
			return rc;
		}

		break;

	case SNDRV_LSM_DEREG_SND_MODEL:
		dev_dbg(rtd->dev,
			"%s: %s\n",
			__func__, "SNDRV_LSM_DEREG_SND_MODEL");
		if (lab_sess->lab_enable) {
			/*
			 * It is possible that lab is still enabled
			 * when trying to deregister sound model.
			 * Make sure to disable lab before de-allocating
			 * the lab buffer.
			 */
			rc = msm_cpe_lsm_lab_stop(substream);
			if (rc) {
				dev_err(rtd->dev,
					"%s: LAB stop failed, error = %d\n",
					__func__, rc);
				return rc;
			}

			rc = lsm_ops->lsm_lab_control(cpe->core_handle,
					session, lab_sess->hw_params.buf_sz,
					lab_sess->hw_params.period_count,
					false);
			if (rc) {
				pr_err("%s: Lab Disable Failed rc %d\n",
				       __func__, rc);
			}
		}

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
		session->snd_model_data = NULL;
		session->conf_levels = NULL;

		rc = lsm_ops->lsm_shmem_dealloc(cpe->core_handle, session);
		if (rc != 0) {
			dev_err(rtd->dev,
				"%s: LSM shared memory dealloc failed, err = %d\n",
				__func__, rc);
			return rc;
		}

		break;

	case SNDRV_LSM_EVENT_STATUS:
		dev_dbg(rtd->dev,
			"%s: %s\n",
			__func__, "SNDRV_LSM_EVENT_STATUS");
		if (!arg) {
			dev_err(rtd->dev,
				"%s: Invalid argument to ioctl %s\n",
				__func__,
				"SNDRV_LSM_EVENT_STATUS");
			return -EINVAL;
		}

		user = arg;

		/*
		 * Release the api lock before wait to allow
		 * other IOCTLs to be invoked while waiting
		 * for event
		 */
		MSM_CPE_LSM_REL_LOCK(&lsm_d->lsm_api_lock,
				     "lsm_api_lock");

		rc = wait_event_freezable(lsm_d->event_wait,
				(atomic_read(&lsm_d->event_avail) == 1) ||
				(atomic_read(&lsm_d->event_stop) == 1));

		MSM_CPE_LSM_GRAB_LOCK(&lsm_d->lsm_api_lock,
				      "lsm_api_lock");

		if (!rc) {
			if (atomic_read(&lsm_d->event_avail) == 1) {
				rc = 0;
				atomic_set(&lsm_d->event_avail, 0);
				if (lsm_d->ev_det_pld_size >
					user->payload_size) {
					dev_err(rtd->dev,
						"%s: avail pld_bytes = %u, needed = %u\n",
						__func__,
						user->payload_size,
						lsm_d->ev_det_pld_size);
					return -EINVAL;
				}

				user->status = lsm_d->ev_det_status;
				user->payload_size = lsm_d->ev_det_pld_size;

				memcpy(user->payload,
				       lsm_d->ev_det_payload,
				       lsm_d->ev_det_pld_size);

			} else if (atomic_read(&lsm_d->event_stop) == 1) {
				dev_dbg(rtd->dev,
					"%s: wait_aborted\n", __func__);
				user->payload_size = 0;
				rc = 0;
			}
		}

		break;

	case SNDRV_LSM_ABORT_EVENT:
		dev_dbg(rtd->dev,
			"%s: %s\n",
			__func__, "SNDRV_LSM_ABORT_EVENT");
		atomic_set(&lsm_d->event_stop, 1);
		wake_up(&lsm_d->event_wait);
		break;

	case SNDRV_LSM_START:
		dev_dbg(rtd->dev,
			"%s: %s\n",
			__func__, "SNDRV_LSM_START");
		rc = lsm_ops->lsm_start(cpe->core_handle, session);
		if (rc != 0) {
			dev_err(rtd->dev,
				"%s: lsm_start fail, err = %d\n",
				__func__, rc);
			return rc;
		}
		session->started = true;
		break;

	case SNDRV_LSM_STOP:
		dev_dbg(rtd->dev,
			"%s: %s, lab_enable = %d, lab_thread_status = %d\n",
			__func__, "SNDRV_LSM_STOP",
			lab_sess->lab_enable,
			lab_sess->thread_status);
		if ((lab_sess->lab_enable &&
		     lab_sess->thread_status ==
		     MSM_LSM_LAB_THREAD_RUNNING)) {
			/* Explicitly stop LAB */
			rc = msm_cpe_lsm_lab_stop(substream);
			if (rc) {
				dev_err(rtd->dev,
					"%s: lab_stop failed, err = %d\n",
					__func__, rc);
				return rc;
			}
		}

		rc = lsm_ops->lsm_stop(cpe->core_handle, session);
		if (rc != 0) {
			dev_err(rtd->dev,
				"%s: lsm_stop fail err = %d\n",
				__func__, rc);

			return rc;
		}
		session->started = false;
		break;

	case SNDRV_LSM_SET_PARAMS:
		if (!arg) {
			dev_err(rtd->dev,
				"%s: %s Invalid argument\n",
				__func__, "SNDRV_LSM_SET_PARAMS");
			return -EINVAL;
		}
		memcpy(&det_params, arg,
			sizeof(det_params));
		if (det_params.num_confidence_levels <= 0) {
			dev_err(rtd->dev,
				"%s: %s: Invalid confidence levels %u\n",
				__func__, "SNDRV_LSM_SET_PARAMS",
				det_params.num_confidence_levels);
			return -EINVAL;
		}

		session->num_confidence_levels =
				det_params.num_confidence_levels;
		rc = msm_cpe_lsm_get_conf_levels(session,
						det_params.conf_level);
		if (rc) {
			dev_err(rtd->dev,
				"%s: %s get_conf_levels fail, err = %d\n",
				__func__, "SNDRV_LSM_SET_PARAMS",
				rc);
			break;
		}

		rc = lsm_ops->lsm_set_data(cpe->core_handle, session,
					   det_params.detect_mode,
					   det_params.detect_failure);
		if (rc) {
			dev_err(rtd->dev,
				"%s: lsm_set_data failed, err = %d\n",
				__func__, rc);
			return rc;
		}

		kfree(session->conf_levels);
		session->conf_levels = NULL;

		break;

	default:
		dev_dbg(rtd->dev,
			"%s: Default snd_lib_ioctl cmd 0x%x\n",
			__func__, cmd);
		rc = snd_pcm_lib_ioctl(substream, cmd, arg);
	}

	return rc;
}

static int msm_cpe_lsm_lab_start(struct snd_pcm_substream *substream,
		struct snd_lsm_event_status *event_status)
{
	struct snd_soc_pcm_runtime *rtd;
	struct cpe_lsm_data *lsm_d = NULL;
	struct cpe_priv *cpe = NULL;
	struct cpe_lsm_session *session = NULL;
	struct wcd_cpe_lsm_lab *lab_sess = NULL;
	struct wcd_cpe_lsm_ops *lsm_ops;

	if (!substream || !substream->private_data) {
		pr_err("%s: invalid substream (%p)\n",
			__func__, substream);
		return -EINVAL;
	}

	rtd = substream->private_data;
	lsm_d = cpe_get_lsm_data(substream);
	cpe = cpe_get_private_data(substream);

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
	lab_sess = &session->lab;

	if (!session->started) {
		dev_dbg(rtd->dev,
			"%s: Session is stopped, cannot start LAB\n",
			__func__);
		return 0;
	}

	INIT_COMPLETION(lab_sess->thread_complete);

	if (lab_sess->lab_enable &&
	    event_status->status ==
	    LSM_VOICE_WAKEUP_STATUS_DETECTED) {

		atomic_set(&lab_sess->abort_read, 0);
		pr_debug("%s: KW detected,\n"
		"scheduling LAB thread\n", __func__);
		lsm_ops->lsm_lab_data_channel_open(
			cpe->core_handle, session);

		/*
		 * Even though thread might be only scheduled and
		 * not currently running, mark the internal driver
		 * status to running so driver can cancel this thread
		 * if it needs to before the thread gets chance to run.
		 */
		lab_sess->thread_status = MSM_LSM_LAB_THREAD_RUNNING;
		session->lsm_lab_thread = kthread_run(
				msm_cpe_lab_thread,
				&session->lab,
				"lab_thread");
	}

	return 0;
}

static int msm_cpe_lsm_ioctl(struct snd_pcm_substream *substream,
			 unsigned int cmd, void *arg)
{
	int err = 0;
	struct snd_soc_pcm_runtime *rtd;
	struct cpe_priv *cpe = NULL;
	struct cpe_lsm_data *lsm_d = NULL;
	struct cpe_lsm_session *session = NULL;
	struct wcd_cpe_lsm_lab *lab_sess = NULL;
	struct wcd_cpe_lsm_ops *lsm_ops;

	if (!substream || !substream->private_data) {
		pr_err("%s: invalid substream (%p)\n",
			__func__, substream);
		return -EINVAL;
	}

	rtd = substream->private_data;
	lsm_d = cpe_get_lsm_data(substream);
	cpe = cpe_get_private_data(substream);

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

	MSM_CPE_LSM_GRAB_LOCK(&lsm_d->lsm_api_lock,
			      "lsm_api_lock");

	session = lsm_d->lsm_session;
	lsm_ops = &cpe->lsm_ops;
	lab_sess = &session->lab;

	switch (cmd) {
	case SNDRV_LSM_REG_SND_MODEL_V2: {
		struct snd_lsm_sound_model_v2 snd_model;
		if (copy_from_user(&snd_model, (void *)arg,
				   sizeof(struct snd_lsm_sound_model_v2))) {
			dev_err(rtd->dev,
				"%s: copy from user failed, size %zd\n",
				__func__,
				sizeof(struct snd_lsm_sound_model_v2));
			err = -EFAULT;
			goto done;
		}

		err = msm_cpe_lsm_ioctl_shared(substream, cmd,
					       &snd_model);
	}
		break;
	case SNDRV_LSM_EVENT_STATUS: {
		struct snd_lsm_event_status u_event_status;
		struct snd_lsm_event_status *event_status = NULL;
		int u_pld_size = 0;

		if (copy_from_user(&u_event_status, (void *)arg,
				   sizeof(struct snd_lsm_event_status))) {
			dev_err(rtd->dev,
				"%s: event status copy from user failed, size %zd\n",
				__func__,
				sizeof(struct snd_lsm_event_status));
			err = -EFAULT;
			goto done;
		}

		if (u_event_status.payload_size >
		    LISTEN_MAX_STATUS_PAYLOAD_SIZE) {
			dev_err(rtd->dev,
				"%s: payload_size %d is invalid, max allowed = %d\n",
				__func__, u_event_status.payload_size,
				LISTEN_MAX_STATUS_PAYLOAD_SIZE);
			err = -EINVAL;
			goto done;
		}

		u_pld_size = sizeof(struct snd_lsm_event_status) +
				u_event_status.payload_size;

		event_status = kzalloc(u_pld_size, GFP_KERNEL);
		if (!event_status) {
			dev_err(rtd->dev,
				"%s: No memory for event status\n",
				__func__);
			err = -ENOMEM;
			goto done;
		} else {
			event_status->payload_size =
				u_event_status.payload_size;
			err = msm_cpe_lsm_ioctl_shared(substream,
						       cmd, event_status);
		}

		if (!err  && copy_to_user(arg, event_status, u_pld_size)) {
			dev_err(rtd->dev,
				"%s: copy to user failed\n",
				__func__);
			kfree(event_status);
			err = -EFAULT;
			goto done;
		}

		msm_cpe_lsm_lab_start(substream, event_status);
		msm_cpe_process_event_status_done(lsm_d);
		kfree(event_status);
	}
		break;
	case SNDRV_LSM_SET_PARAMS: {
		struct snd_lsm_detection_params det_params;

		if (copy_from_user(&det_params, (void *) arg,
				   sizeof(det_params))) {
			dev_err(rtd->dev,
				"%s: %s: copy_from_user failed, size = %zd\n",
				__func__, "SNDRV_LSM_SET_PARAMS",
				sizeof(det_params));
			err = -EFAULT;
			goto done;
		}

		err = msm_cpe_lsm_ioctl_shared(substream, cmd,
					       &det_params);
	}
		break;
	default:
		err = msm_cpe_lsm_ioctl_shared(substream, cmd, arg);
		break;
	}

done:
	MSM_CPE_LSM_REL_LOCK(&lsm_d->lsm_api_lock,
			     "lsm_api_lock");
	return err;
}

#ifdef CONFIG_COMPAT
struct snd_lsm_event_status32 {
	u16 status;
	u16 payload_size;
	u8 payload[0];
};

struct snd_lsm_sound_model_v2_32 {
	compat_uptr_t data;
	compat_uptr_t confidence_level;
	u32 data_size;
	enum lsm_detection_mode detection_mode;
	u8 num_confidence_levels;
	bool detect_failure;
};

struct snd_lsm_detection_params_32 {
	compat_uptr_t conf_level;
	enum lsm_detection_mode detect_mode;
	u8 num_confidence_levels;
	bool detect_failure;
};

enum {
	SNDRV_LSM_EVENT_STATUS32 =
		_IOW('U', 0x02, struct snd_lsm_event_status32),
	SNDRV_LSM_REG_SND_MODEL_V2_32 =
		_IOW('U', 0x07, struct snd_lsm_sound_model_v2_32),
	SNDRV_LSM_SET_PARAMS32 =
		_IOW('U', 0x0A, struct snd_lsm_detection_params_32),
};

static int msm_cpe_lsm_ioctl_compat(struct snd_pcm_substream *substream,
			 unsigned int cmd, void *arg)
{
	int err = 0;
	struct snd_soc_pcm_runtime *rtd;
	struct cpe_priv *cpe = NULL;
	struct cpe_lsm_data *lsm_d = NULL;
	struct cpe_lsm_session *session = NULL;
	struct wcd_cpe_lsm_lab *lab_sess = NULL;
	struct wcd_cpe_lsm_ops *lsm_ops;

	if (!substream || !substream->private_data) {
		pr_err("%s: invalid substream (%p)\n",
			__func__, substream);
		return -EINVAL;
	}

	rtd = substream->private_data;
	lsm_d = cpe_get_lsm_data(substream);
	cpe = cpe_get_private_data(substream);

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

	MSM_CPE_LSM_GRAB_LOCK(&lsm_d->lsm_api_lock,
			      "lsm_api_lock");

	session = lsm_d->lsm_session;
	lsm_ops = &cpe->lsm_ops;
	lab_sess = &session->lab;

	switch (cmd) {
	case SNDRV_LSM_REG_SND_MODEL_V2_32: {
		struct snd_lsm_sound_model_v2 snd_model;
		struct snd_lsm_sound_model_v2_32 snd_model32;

		dev_dbg(rtd->dev,
			"%s: ioctl %s\n", __func__,
			"SNDRV_LSM_REG_SND_MODEL_V2_32");

		if (copy_from_user(&snd_model32, (void *)arg,
				   sizeof(snd_model32))) {
			dev_err(rtd->dev,
				"%s: copy from user failed, size %zd\n",
				__func__,
				sizeof(snd_model32));
			err = -EFAULT;
			goto done;
		}

		snd_model.data = compat_ptr(snd_model32.data);
		snd_model.confidence_level =
			compat_ptr(snd_model32.confidence_level);
		snd_model.data_size = snd_model32.data_size;
		snd_model.detect_failure = snd_model32.detect_failure;
		snd_model.num_confidence_levels =
			snd_model32.num_confidence_levels;
		snd_model.detection_mode = snd_model32.detection_mode;

		cmd = SNDRV_LSM_REG_SND_MODEL_V2;
		err = msm_cpe_lsm_ioctl_shared(substream, cmd, &snd_model);
		if (err)
			dev_err(rtd->dev,
				"%s: %s failed, error = %d\n",
				__func__,
				"SNDRV_LSM_REG_SND_MODEL_V2_32",
				err);
	}
		break;
	case SNDRV_LSM_EVENT_STATUS32: {
		struct snd_lsm_event_status *event_status = NULL;
		struct snd_lsm_event_status u_event_status32;
		struct snd_lsm_event_status *udata_32 = NULL;
		int u_pld_size = 0;

		dev_dbg(rtd->dev,
			"%s: ioctl %s\n", __func__,
			"SNDRV_LSM_EVENT_STATUS32");

		if (copy_from_user(&u_event_status32, (void *)arg,
				   sizeof(struct snd_lsm_event_status))) {
			dev_err(rtd->dev,
				"%s: event status copy from user failed, size %zd\n",
				__func__,
				sizeof(struct snd_lsm_event_status));
			err = -EFAULT;
			goto done;
		}

		if (u_event_status32.payload_size >
		   LISTEN_MAX_STATUS_PAYLOAD_SIZE) {
			dev_err(rtd->dev,
				"%s: payload_size %d is invalid, max allowed = %d\n",
				__func__, u_event_status32.payload_size,
				LISTEN_MAX_STATUS_PAYLOAD_SIZE);
			err = -EINVAL;
			goto done;
		}

		u_pld_size = sizeof(struct snd_lsm_event_status) +
				u_event_status32.payload_size;
		event_status = kzalloc(u_pld_size, GFP_KERNEL);
		if (!event_status) {
			dev_err(rtd->dev,
				"%s: No memory for event status\n",
				__func__);
			err = -ENOMEM;
			goto done;
		} else {
			event_status->payload_size =
				u_event_status32.payload_size;
			cmd = SNDRV_LSM_EVENT_STATUS;
			err = msm_cpe_lsm_ioctl_shared(substream,
						       cmd, event_status);
			if (err)
				dev_err(rtd->dev,
					"%s: %s failed, error = %d\n",
					__func__,
					"SNDRV_LSM_EVENT_STATUS32",
					err);
		}

		if (!err) {
			udata_32 = kzalloc(u_pld_size, GFP_KERNEL);
			if (!udata_32) {
				dev_err(rtd->dev,
					"%s: nomem for udata\n",
					__func__);
				err = -EFAULT;
			} else {
				udata_32->status = event_status->status;
				udata_32->payload_size =
					event_status->payload_size;
				memcpy(udata_32->payload,
				       event_status->payload,
				       u_pld_size);
			}
		}

		if (!err  && copy_to_user(arg, udata_32,
					  u_pld_size)) {
			dev_err(rtd->dev,
				"%s: copy to user failed\n",
				__func__);
			kfree(event_status);
			kfree(udata_32);
			err = -EFAULT;
			goto done;
		}

		msm_cpe_lsm_lab_start(substream, event_status);
		msm_cpe_process_event_status_done(lsm_d);
		kfree(event_status);
		kfree(udata_32);
	}
		break;
	case SNDRV_LSM_SET_PARAMS32: {
		struct snd_lsm_detection_params_32 det_params32;
		struct snd_lsm_detection_params det_params;
		if (copy_from_user(&det_params32, arg,
				   sizeof(det_params32))) {
			err = -EFAULT;
			dev_err(rtd->dev,
				"%s: %s: copy_from_user failed, size = %zd\n",
				__func__, "SNDRV_LSM_SET_PARAMS_32",
				sizeof(det_params32));
		} else {
			det_params.conf_level =
				compat_ptr(det_params32.conf_level);
			det_params.detect_mode =
				det_params32.detect_mode;
			det_params.num_confidence_levels =
				det_params32.num_confidence_levels;
			det_params.detect_failure =
				det_params32.detect_failure;
			cmd = SNDRV_LSM_SET_PARAMS;
			err = msm_cpe_lsm_ioctl_shared(substream, cmd,
						  &det_params);
			if (err)
				dev_err(rtd->dev,
					"%s: ioctl %s failed\n", __func__,
					"SNDRV_LSM_SET_PARAMS");
		}

		break;
	}

	default:
		err = msm_cpe_lsm_ioctl_shared(substream, cmd, arg);
		break;
	}
done:
	MSM_CPE_LSM_REL_LOCK(&lsm_d->lsm_api_lock,
			     "lsm_api_lock");
	return err;
}

#else
#define msm_cpe_lsm_ioctl_compat NULL
#endif

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
	struct cpe_lsm_session *lsm_session;
	struct wcd_cpe_lsm_lab *lab_s = NULL;
	struct snd_pcm_runtime *runtime = substream->runtime;
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
	if (runtime->status->state == SNDRV_PCM_STATE_XRUN ||
	    runtime->status->state == SNDRV_PCM_STATE_PREPARED) {
		pr_err("%s: XRUN ignore for now\n", __func__);
		return 0;
	}

	lsm_session = lsm_d->lsm_session;
	lab_s = &lsm_session->lab;
	lab_s->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	pr_debug("%s: pcm_size 0x%x", __func__, lab_s->pcm_size);
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

static int msm_cpe_lsm_hwparams(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{

	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct cpe_lsm_data *lsm_d = cpe_get_lsm_data(substream);
	struct cpe_priv *cpe = cpe_get_private_data(substream);
	struct cpe_lsm_session *session = NULL;
	struct wcd_cpe_lab_hw_params *lab_hw_params;

	if (!cpe || !cpe->core_handle) {
		dev_err(rtd->dev,
			"%s: Invalid private data\n",
			__func__);
		return -EINVAL;
	}

	if (!lsm_d) {
		dev_err(rtd->dev,
			"%s: Invalid session data\n",
			__func__);
		return -EINVAL;
	}
	session = lsm_d->lsm_session;
	lab_hw_params = &session->lab.hw_params;
	lab_hw_params->buf_sz = (params_buffer_bytes(params)
				/ params_periods(params));
	lab_hw_params->period_count = params_periods(params);
	lab_hw_params->sample_rate = params_rate(params);
	if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE)
		lab_hw_params->sample_size = 16;
	else {
		pr_err("%s: Invalid Format\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: Format %d buffer size(bytes) %d period count %d\n"
		 " Channel %d period in bytes 0x%x Period Size 0x%x\n",
		 __func__, params_format(params), params_buffer_bytes(params),
		 params_periods(params), params_channels(params),
		 params_period_bytes(params), params_period_size(params));
return 0;
}

static snd_pcm_uframes_t msm_cpe_lsm_pointer(
				struct snd_pcm_substream *substream)
{

	struct cpe_lsm_data *lsm_d = cpe_get_lsm_data(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct cpe_lsm_session *session;
	struct wcd_cpe_lsm_lab *lab_s = NULL;

	session = lsm_d->lsm_session;
	lab_s = &session->lab;
	if (lab_s->dma_write  >= lab_s->pcm_size)
		lab_s->dma_write = 0;
	pr_debug("%s:pcm_dma_pos = %d\n", __func__, lab_s->dma_write);
	return bytes_to_frames(runtime, (lab_s->dma_write));
}

static int msm_cpe_lsm_copy(struct snd_pcm_substream *substream, int a,
	 snd_pcm_uframes_t hwoff, void __user *buf, snd_pcm_uframes_t frames)
{
	struct cpe_lsm_data *lsm_d = cpe_get_lsm_data(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct cpe_lsm_session *session;
	struct wcd_cpe_lsm_lab *lab_s = NULL;
	char *pcm_buf;
	int fbytes = 0;
	int rc = 0;

	fbytes = frames_to_bytes(runtime, frames);
	if (runtime->status->state == SNDRV_PCM_STATE_XRUN ||
	   runtime->status->state == SNDRV_PCM_STATE_PREPARED) {
		pr_err("%s: XRUN ignore for now\n", __func__);
		return 0;
	}
	session = lsm_d->lsm_session;
	lab_s = &session->lab;

	/* Check if buffer reading is already in error state */
	if (lab_s->thread_status != MSM_LSM_LAB_THREAD_RUNNING) {
		pr_err("%s: Buffers not available\n",
			__func__);
		/*
		 * Advance the period so there is no wait in case
		 * read is invoked even after error is propogated
		 */
		atomic_inc(&lab_s->in_count);
		lab_s->dma_write += snd_pcm_lib_period_bytes(substream);
		snd_pcm_period_elapsed(substream);
		return -ENETRESET;
	}

	rc = wait_event_timeout(lab_s->period_wait,
			(atomic_read(&lab_s->in_count) ||
			atomic_read(&lab_s->abort_read)),
			(2 * HZ));
	if (atomic_read(&lab_s->abort_read)) {
		pr_debug("%s: LSM LAB Abort read\n", __func__);
		return -EIO;
	}
	if (lab_s->thread_status != MSM_LSM_LAB_THREAD_RUNNING) {
		pr_err("%s: Lab stopped\n", __func__);
		return -EIO;
	}
	if (!rc) {
		pr_err("%s:LAB err wait_event_timeout\n", __func__);
		rc = -EAGAIN;
		goto fail;
	}
	if (lab_s->buf_idx >= (lab_s->hw_params.period_count))
		lab_s->buf_idx = 0;
	pcm_buf = (lab_s->pcm_buf[lab_s->buf_idx].mem);
	pr_debug("%s: Buf IDX = 0x%x pcm_buf %pa\n",
			__func__,
			lab_s->buf_idx,
			&(lab_s->pcm_buf[lab_s->buf_idx]));
	if (pcm_buf) {
		if (copy_to_user(buf, pcm_buf, fbytes)) {
			pr_err("Failed to copy buf to user\n");
			rc = -EFAULT;
			goto fail;
		}
	}
	lab_s->buf_idx++;
	atomic_dec(&lab_s->in_count);
	return 0;
fail:
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
	.pointer = msm_cpe_lsm_pointer,
	.copy = msm_cpe_lsm_copy,
	.hw_params = msm_cpe_lsm_hwparams,
	.compat_ioctl = msm_cpe_lsm_ioctl_compat,
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
