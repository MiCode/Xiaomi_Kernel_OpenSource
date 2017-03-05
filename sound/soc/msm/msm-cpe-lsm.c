/*
 * Copyright (c) 2013-2016, Linux Foundation. All rights reserved.
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
#include <sound/msm-slim-dma.h>

#define SAMPLE_RATE_48KHZ 48000
#define SAMPLE_RATE_16KHZ 16000
#define LSM_VOICE_WAKEUP_APP_V2 2
#define AFE_PORT_ID_1 1
#define AFE_PORT_ID_3 3
#define AFE_OUT_PORT_2 2
#define LISTEN_MIN_NUM_PERIODS     2
#define LISTEN_MAX_NUM_PERIODS     12
#define LISTEN_MAX_PERIOD_SIZE     61440
#define LISTEN_MIN_PERIOD_SIZE     320
#define LISTEN_MAX_STATUS_PAYLOAD_SIZE 256
#define MSM_CPE_MAX_CUSTOM_PARAM_SIZE 2048

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
	8000, 16000, 48000, 192000, 384000
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
	.formats = (SNDRV_PCM_FMTBIT_S16_LE |
		    SNDRV_PCM_FMTBIT_S24_LE |
		    SNDRV_PCM_FMTBIT_S32_LE),
	.rates = (SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000 |
		  SNDRV_PCM_RATE_192000 | SNDRV_PCM_RATE_384000),
	.rate_min = 16000,
	.rate_max = 384000,
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

enum cpe_lab_thread_status {
	MSM_LSM_LAB_THREAD_STOP,
	MSM_LSM_LAB_THREAD_RUNNING,
	MSM_LSM_LAB_THREAD_ERROR,
};

struct cpe_hw_params {
	u32 sample_rate;
	u16 sample_size;
	u32 buf_sz;
	u32 period_count;
	u16 channels;
};

struct cpe_data_pcm_buf {
	u8 *mem;
	phys_addr_t phys;
};

struct cpe_lsm_lab {
	atomic_t in_count;
	atomic_t abort_read;
	u32 dma_write;
	u32 buf_idx;
	u32 pcm_size;
	enum cpe_lab_thread_status thread_status;
	struct cpe_data_pcm_buf *pcm_buf;
	wait_queue_head_t period_wait;
	struct completion comp;
	struct completion thread_complete;
};

struct cpe_priv {
	void *core_handle;
	struct snd_soc_codec *codec;
	struct wcd_cpe_lsm_ops lsm_ops;
	struct wcd_cpe_afe_ops afe_ops;
	bool afe_mad_ctl;
	u32 input_port_id;
};

struct cpe_lsm_data {
	struct device *dev;
	struct cpe_lsm_session *lsm_session;
	struct mutex lsm_api_lock;
	struct cpe_lsm_lab lab;
	struct cpe_hw_params hw_params;
	struct snd_pcm_substream *substream;

	wait_queue_head_t event_wait;
	atomic_t event_avail;
	atomic_t event_stop;

	u8 ev_det_status;
	u8 ev_det_pld_size;
	u8 *ev_det_payload;

	bool cpe_prepared;
};

static int msm_cpe_afe_mad_ctl_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct cpe_priv *cpe = kcontrol->private_data;

	ucontrol->value.integer.value[0] = cpe->afe_mad_ctl;
	return 0;
}

static int msm_cpe_afe_mad_ctl_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct cpe_priv *cpe = kcontrol->private_data;

	cpe->afe_mad_ctl = ucontrol->value.integer.value[0];
	return 0;
}

static struct snd_kcontrol_new msm_cpe_kcontrols[] = {
	SOC_SINGLE_EXT("CPE AFE MAD Enable", SND_SOC_NOPM, 0, 1, 0,
			msm_cpe_afe_mad_ctl_get, msm_cpe_afe_mad_ctl_put),
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

static void msm_cpe_process_event_status(void *data,
		u8 detect_status, u8 size, u8 *payload)
{
	struct cpe_lsm_data *lsm_d = data;

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
		/*
		 * It is possible driver can get closed without prepare,
		 * in which case afe ports will not be initialized.
		 */
		dev_dbg(rtd->dev,
			"%s: Invalid afe port id\n",
			__func__);
		return 0;
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
	struct wcd_cpe_afe_ops *afe_ops;
	struct cpe_lsm_session *session;
	struct cpe_lsm_lab *lab_d = &lsm_d->lab;
	struct msm_slim_dma_data *dma_data = NULL;
	int rc;

	/*
	 * the caller is not aware of LAB status and will
	 * try to stop lab even if it is already stopped.
	 * return success right away is LAB is already stopped
	 */
	if (lab_d->thread_status == MSM_LSM_LAB_THREAD_STOP) {
		dev_dbg(rtd->dev,
			"%s: lab already stopped\n",
			__func__);
		return 0;
	}

	if (!cpe || !cpe->core_handle) {
		dev_err(rtd->dev,
			"%s: Invalid private data\n",
			__func__);
		return -EINVAL;
	}

	if (!lsm_d->lsm_session) {
		dev_err(rtd->dev,
			"%s: Invalid session data\n",
			__func__);
		return -EINVAL;
	}

	lsm_ops = &cpe->lsm_ops;
	afe_ops = &cpe->afe_ops;
	session = lsm_d->lsm_session;
	if (rtd->cpu_dai)
		dma_data = snd_soc_dai_get_dma_data(rtd->cpu_dai,
					substream);
	if (!dma_data || !dma_data->dai_channel_ctl) {
		dev_err(rtd->dev,
			"%s: dma_data is not set\n",
			__func__);
		return -EINVAL;
	}

	if (lab_d->thread_status == MSM_LSM_LAB_THREAD_RUNNING) {
		dev_dbg(rtd->dev, "%s: stopping lab thread\n",
			__func__);
		rc = kthread_stop(session->lsm_lab_thread);

		/*
		 * kthread_stop returns EINTR if the thread_fn
		 * was not scheduled before calling kthread_stop.
		 * In this case, we dont need to wait for lab
		 * thread to complete as lab thread will not be
		 * scheduled at all.
		 */
		if (rc == -EINTR)
			goto done;

		/* Wait for the lab thread to exit */
		rc = wait_for_completion_timeout(
				&lab_d->thread_complete,
				MSM_CPE_LAB_THREAD_TIMEOUT);
		if (!rc) {
			dev_err(rtd->dev,
				"%s: Wait for lab thread timedout\n",
				__func__);
			return -ETIMEDOUT;
		}
	}

	rc = dma_data->dai_channel_ctl(dma_data, rtd->cpu_dai,
				       MSM_DAI_SLIM_PRE_DISABLE);
	if (rc)
		dev_err(rtd->dev,
			"%s: PRE_DISABLE failed, err = %d\n",
			__func__, rc);

	/* continue with teardown even if any intermediate step fails */
	rc = lsm_ops->lab_ch_setup(cpe->core_handle,
				   session,
				   WCD_CPE_PRE_DISABLE);
	if (rc)
		dev_err(rtd->dev,
			"%s: PRE ch teardown failed, err = %d\n",
			__func__, rc);

	rc = dma_data->dai_channel_ctl(dma_data, rtd->cpu_dai,
				       MSM_DAI_SLIM_DISABLE);
	if (rc)
		dev_err(rtd->dev,
			"%s: DISABLE failed, err = %d\n",
			__func__, rc);
	dma_data->ph = 0;

	/*
	 * Even though LAB stop failed,
	 * output AFE port needs to be stopped
	 */
	rc = afe_ops->afe_port_stop(cpe->core_handle,
				    &session->afe_out_port_cfg);
	if (rc)
		dev_err(rtd->dev,
			"%s: AFE out port stop failed, err = %d\n",
			__func__, rc);

	rc = lsm_ops->lab_ch_setup(cpe->core_handle,
				   session,
				   WCD_CPE_POST_DISABLE);
	if (rc)
		dev_err(rtd->dev,
			"%s: POST ch teardown failed, err = %d\n",
			__func__, rc);

done:
	lab_d->thread_status = MSM_LSM_LAB_THREAD_STOP;
	lab_d->buf_idx = 0;
	atomic_set(&lab_d->in_count, 0);
	lab_d->dma_write = 0;

	return 0;
}

static int msm_cpe_lab_buf_alloc(struct snd_pcm_substream *substream,
		struct cpe_lsm_session *session,
		struct msm_slim_dma_data *dma_data)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct cpe_lsm_data *lsm_d = cpe_get_lsm_data(substream);
	struct cpe_lsm_lab *lab_d = &lsm_d->lab;
	struct cpe_hw_params *hw_params = &lsm_d->hw_params;
	struct cpe_data_pcm_buf *pcm_buf = NULL;
	int rc = 0;
	int dma_alloc = 0;
	u32 count = 0;
	u32 bufsz, bufcnt;

	if (lab_d->pcm_buf &&
	    lab_d->pcm_buf->mem) {
		dev_dbg(rtd->dev,
			"%s: LAB buf already allocated\n",
			__func__);
		goto exit;
	}

	bufsz = hw_params->buf_sz;
	bufcnt = hw_params->period_count;

	dev_dbg(rtd->dev,
		"%s:Buf Size %d Buf count %d\n",
		 __func__,
		bufsz, bufcnt);

	pcm_buf = kzalloc(((sizeof(struct cpe_data_pcm_buf)) * bufcnt),
			  GFP_KERNEL);
	if (!pcm_buf) {
		rc = -ENOMEM;
		goto exit;
	}

	lab_d->pcm_buf = pcm_buf;
	dma_alloc = bufsz * bufcnt;
	pcm_buf->mem = NULL;
	pcm_buf->mem = dma_alloc_coherent(dma_data->sdev->dev.parent,
					  dma_alloc,
					  &(pcm_buf->phys),
					  GFP_KERNEL);
	if (!pcm_buf->mem) {
		dev_err(rtd->dev,
			"%s:DMA alloc failed size = %x\n",
			__func__, dma_alloc);
		rc = -ENOMEM;
		goto fail;
	}

	count = 0;
	while (count < bufcnt) {
		pcm_buf[count].mem = pcm_buf[0].mem + (count * bufsz);
		pcm_buf[count].phys = pcm_buf[0].phys + (count * bufsz);
		dev_dbg(rtd->dev,
			"%s: pcm_buf[%d].mem %pK pcm_buf[%d].phys %pK\n",
			 __func__, count,
			(void *)pcm_buf[count].mem,
			count, &(pcm_buf[count].phys));
		count++;
	}

	return 0;
fail:
	if (pcm_buf) {
		if (pcm_buf->mem)
			dma_free_coherent(dma_data->sdev->dev.parent, dma_alloc,
					  pcm_buf->mem, pcm_buf->phys);
		kfree(pcm_buf);
		lab_d->pcm_buf = NULL;
	}
exit:
	return rc;
}

static int msm_cpe_lab_buf_dealloc(struct snd_pcm_substream *substream,
	struct cpe_lsm_session *session, struct msm_slim_dma_data *dma_data)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct cpe_lsm_data *lsm_d = cpe_get_lsm_data(substream);
	struct cpe_lsm_lab *lab_d = &lsm_d->lab;
	struct cpe_hw_params *hw_params = &lsm_d->hw_params;
	int rc = 0;
	int dma_alloc = 0;
	struct cpe_data_pcm_buf *pcm_buf = NULL;
	int bufsz, bufcnt;

	bufsz = hw_params->buf_sz;
	bufcnt = hw_params->period_count;

	dev_dbg(rtd->dev,
		"%s:Buf Size %d Buf count %d\n", __func__,
		bufsz, bufcnt);

	if (bufcnt <= 0 || bufsz <= 0) {
		dev_err(rtd->dev,
			"%s: Invalid params, bufsz = %u, bufcnt = %u\n",
			__func__, bufsz, bufcnt);
		return -EINVAL;
	}

	pcm_buf = lab_d->pcm_buf;
	dma_alloc = bufsz * bufcnt;
	if (dma_data && pcm_buf)
		dma_free_coherent(dma_data->sdev->dev.parent, dma_alloc,
				  pcm_buf->mem, pcm_buf->phys);
	kfree(pcm_buf);
	lab_d->pcm_buf = NULL;
	return rc;
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
	struct cpe_lsm_data *lsm_d = data;
	struct cpe_lsm_session *session = lsm_d->lsm_session;
	struct snd_pcm_substream *substream = lsm_d->substream;
	struct cpe_lsm_lab *lab_d = &lsm_d->lab;
	struct cpe_hw_params *hw_params = &lsm_d->hw_params;
	struct cpe_priv *cpe = cpe_get_private_data(substream);
	struct wcd_cpe_lsm_ops *lsm_ops;
	struct wcd_cpe_afe_ops *afe_ops;
	struct cpe_data_pcm_buf *cur_buf, *next_buf;
	struct msm_slim_dma_data *dma_data = NULL;
	struct snd_soc_pcm_runtime *rtd = NULL;
	bool wait_timedout = false;
	int rc = 0;
	u32 done_len = 0;
	u32 buf_count = 0;
	u32 prd_cnt;

	allow_signal(SIGKILL);
	set_current_state(TASK_INTERRUPTIBLE);

	pr_debug("%s: Lab thread start\n", __func__);
	init_completion(&lab_d->comp);

	if (PCM_RUNTIME_CHECK(substream)) {
		rc = -EINVAL;
		goto done;
	}

	if (!cpe || !cpe->core_handle) {
		pr_err("%s: Handle to %s is invalid\n",
			__func__,
			(!cpe) ? "cpe" : "core");
		rc = -EINVAL;
		goto done;
	}

	rtd = substream->private_data;
	if (rtd->cpu_dai)
		dma_data = snd_soc_dai_get_dma_data(rtd->cpu_dai,
					substream);
	if (!dma_data || !dma_data->dai_channel_ctl) {
		pr_err("%s: dma_data is not set\n", __func__);
		rc = -EINVAL;
		goto done;
	}

	lsm_ops = &cpe->lsm_ops;
	afe_ops = &cpe->afe_ops;

	rc = lsm_ops->lab_ch_setup(cpe->core_handle,
				   session,
				   WCD_CPE_PRE_ENABLE);
	if (rc) {
		dev_err(rtd->dev,
			"%s: PRE ch setup failed, err = %d\n",
			__func__, rc);
		goto done;
	}

	rc = dma_data->dai_channel_ctl(dma_data, rtd->cpu_dai,
				       MSM_DAI_SLIM_ENABLE);
	if (rc) {
		dev_err(rtd->dev,
			"%s: open data failed %d\n", __func__, rc);
		goto done;
	}

	dev_dbg(rtd->dev, "%s: Established data channel\n",
		__func__);

	init_waitqueue_head(&lab_d->period_wait);
	memset(lab_d->pcm_buf[0].mem, 0, lab_d->pcm_size);

	rc = slim_port_xfer(dma_data->sdev, dma_data->ph,
			    lab_d->pcm_buf[0].phys,
			    hw_params->buf_sz, &lab_d->comp);
	if (rc) {
		dev_err(rtd->dev,
			"%s: buf[0] slim_port_xfer failed, err = %d\n",
			__func__, rc);
		goto done;
	}

	rc = slim_port_xfer(dma_data->sdev, dma_data->ph,
			    lab_d->pcm_buf[1].phys,
			    hw_params->buf_sz, &lab_d->comp);
	if (rc) {
		dev_err(rtd->dev,
			"%s: buf[0] slim_port_xfer failed, err = %d\n",
			__func__, rc);
		goto done;
	}

	cur_buf = &lab_d->pcm_buf[0];
	next_buf = &lab_d->pcm_buf[2];
	prd_cnt = hw_params->period_count;
	rc = lsm_ops->lab_ch_setup(cpe->core_handle,
				   session,
				   WCD_CPE_POST_ENABLE);
	if (rc) {
		dev_err(rtd->dev,
			"%s: POST ch setup failed, err = %d\n",
			__func__, rc);
		goto done;
	}

	rc = afe_ops->afe_port_start(cpe->core_handle,
			&session->afe_out_port_cfg);
	if (rc) {
		dev_err(rtd->dev,
			"%s: AFE out port start failed, err = %d\n",
			__func__, rc);
		goto done;
	}

	while (!kthread_should_stop() &&
	       lab_d->thread_status != MSM_LSM_LAB_THREAD_ERROR) {

		rc = slim_port_xfer(dma_data->sdev, dma_data->ph,
				    next_buf->phys,
				    hw_params->buf_sz, &lab_d->comp);
		if (rc) {
			dev_err(rtd->dev,
				"%s: slim_port_xfer failed, err = %d\n",
				__func__, rc);
			lab_d->thread_status = MSM_LSM_LAB_THREAD_ERROR;
		}

		rc = wait_for_completion_timeout(&lab_d->comp, (2 * HZ/10));
		if (!rc) {
			dev_err(rtd->dev,
				"%s: wait timedout for slim buffer\n",
				__func__);
			wait_timedout = true;
		} else {
			wait_timedout = false;
		}

		rc = slim_port_get_xfer_status(dma_data->sdev,
					       dma_data->ph,
					       &cur_buf->phys, &done_len);
		if (rc ||
		    (!rc && wait_timedout)) {
			dev_err(rtd->dev,
				"%s: xfer_status failure, rc = %d, wait_timedout = %s\n",
				__func__, rc,
				(wait_timedout ? "true" : "false"));
			lab_d->thread_status = MSM_LSM_LAB_THREAD_ERROR;
		}

		if (done_len ||
		    ((!done_len) &&
		     lab_d->thread_status == MSM_LSM_LAB_THREAD_ERROR)) {
			atomic_inc(&lab_d->in_count);
			lab_d->dma_write += snd_pcm_lib_period_bytes(substream);
			snd_pcm_period_elapsed(substream);
			wake_up(&lab_d->period_wait);
			buf_count++;

			cur_buf = &lab_d->pcm_buf[buf_count % prd_cnt];
			next_buf = &lab_d->pcm_buf[(buf_count + 2) % prd_cnt];
			dev_dbg(rtd->dev,
				"%s: Cur buf.mem = %pK Next Buf.mem = %pK\n"
				" buf count = 0x%x\n", __func__,
				cur_buf->mem, next_buf->mem, buf_count);
		} else {
			dev_err(rtd->dev,
				"%s: SB get status, invalid len = 0x%x\n",
				__func__, done_len);
		}
		done_len = 0;
	}

done:
	if (rc)
		lab_d->thread_status = MSM_LSM_LAB_THREAD_ERROR;
	pr_debug("%s: Exit lab_thread, exit_status=%d, thread_status=%d\n",
		 __func__, rc, lab_d->thread_status);
	complete(&lab_d->thread_complete);

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
	lsm_d->lab.thread_status = MSM_LSM_LAB_THREAD_STOP;
	lsm_d->lsm_session->started = false;
	lsm_d->substream = substream;
	init_waitqueue_head(&lsm_d->lab.period_wait);
	lsm_d->cpe_prepared = false;

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

	lsm_d->cpe_prepared = false;

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

static int msm_cpe_lsm_validate_out_format(
	struct snd_pcm_substream *substream,
	struct snd_lsm_output_format_cfg *cfg)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int rc = 0;

	if (!cfg) {
		dev_err(rtd->dev,
			"%s: Invalid lsm out cfg\n", __func__);
		rc = -EINVAL;
		goto done;
	}

	if (cfg->format != LSM_OUT_FORMAT_PCM &&
	    cfg->format != LSM_OUT_FORMAT_ADPCM) {
		dev_err(rtd->dev,
			"%s: Invalid format %u\n",
			__func__, cfg->format);
		rc = -EINVAL;
		goto done;
	}

	if (cfg->packing != LSM_OUT_DATA_RAW &&
	    cfg->packing != LSM_OUT_DATA_PACKED) {
		dev_err(rtd->dev,
			"%s: Invalid packing method %u\n",
			__func__, cfg->packing);
		rc = -EINVAL;
		goto done;
	}

	if (cfg->events != LSM_OUT_DATA_EVENTS_DISABLED &&
	    cfg->events != LSM_OUT_DATA_EVENTS_ENABLED) {
		dev_err(rtd->dev,
			"%s: Invalid events provided %u\n",
			__func__, cfg->events);
		rc = -EINVAL;
		goto done;
	}

	if (cfg->mode != LSM_OUT_TRANSFER_MODE_RT &&
	    cfg->mode != LSM_OUT_TRANSFER_MODE_FTRT) {
		dev_err(rtd->dev,
			"%s: Invalid transfer mode %u\n",
			__func__, cfg->mode);
		rc = -EINVAL;
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
	struct cpe_lsm_lab *lab_d = &lsm_d->lab;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct msm_slim_dma_data *dma_data = NULL;
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

	switch (cmd) {
	case SNDRV_LSM_STOP_LAB:
		dev_dbg(rtd->dev,
			"%s: %s, lab_enable = %d, lab_thread_ststus = %d\n",
			__func__, "SNDRV_LSM_STOP_LAB",
			session->lab_enable,
			lab_d->thread_status);

		if (session->lab_enable &&
		    lab_d->thread_status != MSM_LSM_LAB_THREAD_STOP) {
			atomic_inc(&lab_d->abort_read);
			wake_up(&lab_d->period_wait);
			rc = msm_cpe_lsm_lab_stop(substream);
			if (rc) {
				dev_err(rtd->dev,
					"%s: stop LAB failed, error = %d\n",
					__func__, rc);
				return rc;
			}
		} else if (!session->lab_enable) {
			dev_dbg(rtd->dev,
				"%s: LAB already stopped\n",
				__func__);
		}

		break;

	case SNDRV_LSM_LAB_CONTROL:
		if (copy_from_user(&session->lab_enable, (void *)arg,
				   sizeof(u32))) {
			dev_err(rtd->dev,
				"%s: copy_from_user failed, size %zd\n",
				__func__, sizeof(u32));
			return -EFAULT;
		}

		dev_dbg(rtd->dev,
			"%s: %s, lab_enable = %d\n",
			__func__, "SNDRV_LSM_LAB_CONTROL",
			session->lab_enable);
		if (rtd->cpu_dai)
			dma_data = snd_soc_dai_get_dma_data(rtd->cpu_dai,
						substream);
		if (!dma_data || !dma_data->dai_channel_ctl) {
			dev_err(rtd->dev,
				"%s: dma_data is not set\n", __func__);
			return -EINVAL;
		}

		if (session->lab_enable) {
			rc = msm_cpe_lab_buf_alloc(substream,
						   session, dma_data);
			if (IS_ERR_VALUE(rc)) {
				dev_err(rtd->dev,
					"%s: lab buffer alloc failed, err = %d\n",
					__func__, rc);
				return rc;
			}

			dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
			dma_buf->dev.dev = substream->pcm->card->dev;
			dma_buf->private_data = NULL;
			dma_buf->area = lab_d->pcm_buf[0].mem;
			dma_buf->addr =  lab_d->pcm_buf[0].phys;
			dma_buf->bytes = (lsm_d->hw_params.buf_sz *
					lsm_d->hw_params.period_count);
			init_completion(&lab_d->thread_complete);
			snd_pcm_set_runtime_buffer(substream,
						   &substream->dma_buffer);
			rc = lsm_ops->lsm_lab_control(cpe->core_handle,
					session, true);
			if (IS_ERR_VALUE(rc)) {
				dev_err(rtd->dev,
					"%s: Lab Enable Failed rc %d\n",
					__func__, rc);
				return rc;
			}
		} else {
			/*
			 * It is possible that lab is still enabled
			 * when trying to de-allocate the lab buffer.
			 * Make sure to disable lab before de-allocating
			 * the lab buffer.
			 */
			rc = msm_cpe_lsm_lab_stop(substream);
			if (IS_ERR_VALUE(rc)) {
				dev_err(rtd->dev,
					"%s: LAB stop failed, error = %d\n",
					__func__, rc);
				return rc;
			}
			/*
			 * Buffer has to be de-allocated even if
			 * lab_control failed.
			 */
			rc = msm_cpe_lab_buf_dealloc(substream,
						     session, dma_data);
			if (IS_ERR_VALUE(rc)) {
				dev_err(rtd->dev,
					"%s: lab buffer free failed, err = %d\n",
					__func__, rc);
				return rc;
			}
		}
	break;
	case SNDRV_LSM_REG_SND_MODEL_V2:
		dev_dbg(rtd->dev,
			"%s: %s\n",
			__func__, "SNDRV_LSM_REG_SND_MODEL_V2");

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

		if (session->lab_enable) {
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
					session, false);
			if (rc)
				dev_err(rtd->dev,
					"%s: Lab Disable Failed rc %d\n",
				       __func__, rc);

			dma_data = snd_soc_dai_get_dma_data(rtd->cpu_dai,
							substream);
			if (!dma_data || !dma_data->dai_channel_ctl)
				dev_err(rtd->dev,
					"%s: dma_data is not set\n", __func__);

			/*
			 * Buffer has to be de-allocated even if
			 * lab_control failed and/or dma data is invalid.
			 */
			rc = msm_cpe_lab_buf_dealloc(substream,
						session, dma_data);
			if (IS_ERR_VALUE(rc))
				dev_err(rtd->dev,
					"%s: lab buffer free failed, err = %d\n",
					__func__, rc);
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
			session->lab_enable,
			lab_d->thread_status);
		if ((session->lab_enable &&
		     lab_d->thread_status ==
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

		case SNDRV_LSM_OUT_FORMAT_CFG: {
			struct snd_lsm_output_format_cfg u_fmt_cfg;

			if (!arg) {
				dev_err(rtd->dev,
					"%s: Invalid argument to ioctl %s\n",
					__func__, "SNDRV_LSM_OUT_FORMAT_CFG");
				return -EINVAL;
			}

			if (copy_from_user(&u_fmt_cfg, arg,
					   sizeof(u_fmt_cfg))) {
				dev_err(rtd->dev,
					"%s: copy_from_user failed for out_fmt_cfg\n",
					__func__);
				return -EFAULT;
			}

			if (msm_cpe_lsm_validate_out_format(substream,
							    &u_fmt_cfg))
				return -EINVAL;

			session->out_fmt_cfg.format = u_fmt_cfg.format;
			session->out_fmt_cfg.pack_mode = u_fmt_cfg.packing;
			session->out_fmt_cfg.data_path_events =
						u_fmt_cfg.events;
			session->out_fmt_cfg.transfer_mode = u_fmt_cfg.mode;

			rc = lsm_ops->lsm_set_fmt_cfg(cpe->core_handle,
						      session);
			if (rc) {
				dev_err(rtd->dev,
					"%s: lsm_set_fmt_cfg failed, err = %d\n",
					__func__, rc);
				return rc;
			}
		}
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
	struct cpe_lsm_lab *lab_d = NULL;
	struct cpe_hw_params *hw_params;
	struct wcd_cpe_lsm_ops *lsm_ops;
	struct wcd_cpe_afe_ops *afe_ops;
	struct wcd_cpe_afe_port_cfg *out_port;
	int rc;

	if (!substream || !substream->private_data) {
		pr_err("%s: invalid substream (%pK)\n",
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
	lab_d = &lsm_d->lab;
	afe_ops = &cpe->afe_ops;
	hw_params = &lsm_d->hw_params;

	if (!session->started) {
		dev_dbg(rtd->dev,
			"%s: Session is stopped, cannot start LAB\n",
			__func__);
		return 0;
	}

	reinit_completion(&lab_d->thread_complete);

	if (session->lab_enable &&
	    event_status->status ==
	    LSM_VOICE_WAKEUP_STATUS_DETECTED) {
		out_port = &session->afe_out_port_cfg;
		out_port->port_id = session->afe_out_port_id;
		out_port->bit_width = hw_params->sample_size;
		out_port->num_channels = hw_params->channels;
		out_port->sample_rate = hw_params->sample_rate;
		dev_dbg(rtd->dev, "%s: port_id= %u, bit_width= %u, rate= %u\n",
			 __func__, out_port->port_id, out_port->bit_width,
			out_port->sample_rate);

		rc = afe_ops->afe_port_cmd_cfg(cpe->core_handle,
					       out_port);
		if (rc) {
			dev_err(rtd->dev,
				"%s: Failed afe generic config v2, err = %d\n",
				__func__, rc);
			return rc;
		}

		atomic_set(&lab_d->abort_read, 0);
		dev_dbg(rtd->dev,
			"%s: KW detected, scheduling LAB thread\n",
			__func__);

		/*
		 * Even though thread might be only scheduled and
		 * not currently running, mark the internal driver
		 * status to running so driver can cancel this thread
		 * if it needs to before the thread gets chance to run.
		 */
		lab_d->thread_status = MSM_LSM_LAB_THREAD_RUNNING;
		session->lsm_lab_thread = kthread_run(
				msm_cpe_lab_thread,
				lsm_d,
				"lab_thread");
	}

	return 0;
}

static bool msm_cpe_lsm_is_valid_stream(struct snd_pcm_substream *substream,
		const char *func)
{
	struct snd_soc_pcm_runtime *rtd;
	struct cpe_lsm_data *lsm_d = NULL;
	struct cpe_priv *cpe = NULL;
	struct cpe_lsm_session *session = NULL;
	struct wcd_cpe_lsm_ops *lsm_ops;

	if (!substream || !substream->private_data) {
		pr_err("%s: invalid substream (%pK)\n",
			func, substream);
		return false;
	}

	rtd = substream->private_data;
	lsm_d = cpe_get_lsm_data(substream);
	cpe = cpe_get_private_data(substream);

	if (!cpe || !cpe->core_handle) {
		dev_err(rtd->dev,
			"%s: Invalid private data\n",
			func);
		return false;
	}

	if (!lsm_d || !lsm_d->lsm_session) {
		dev_err(rtd->dev,
			"%s: Invalid session data\n",
			func);
		return false;
	}

	session = lsm_d->lsm_session;
	lsm_ops = &cpe->lsm_ops;

	if (!lsm_ops) {
		dev_err(rtd->dev,
			"%s: Invalid lsm_ops\n", func);
		return false;
	}

	return true;
}

static int msm_cpe_lsm_set_epd(struct snd_pcm_substream *substream,
		struct lsm_params_info *p_info)
{
	struct snd_soc_pcm_runtime *rtd;
	struct cpe_lsm_data *lsm_d = NULL;
	struct cpe_priv *cpe = NULL;
	struct cpe_lsm_session *session = NULL;
	struct wcd_cpe_lsm_ops *lsm_ops;
	struct snd_lsm_ep_det_thres epd_thres;
	int rc;

	if (!msm_cpe_lsm_is_valid_stream(substream, __func__))
		return -EINVAL;

	rtd = substream->private_data;
	lsm_d = cpe_get_lsm_data(substream);
	cpe = cpe_get_private_data(substream);
	session = lsm_d->lsm_session;
	lsm_ops = &cpe->lsm_ops;

	if (p_info->param_size != sizeof(epd_thres)) {
		dev_err(rtd->dev,
			"%s: Invalid param_size %d\n",
			__func__, p_info->param_size);
		rc = -EINVAL;
		goto done;
	}

	if (copy_from_user(&epd_thres, p_info->param_data,
			   p_info->param_size)) {
		dev_err(rtd->dev,
			"%s: copy_from_user failed, size = %d\n",
			__func__, p_info->param_size);
		rc = -EFAULT;
		goto done;
	}

	rc = lsm_ops->lsm_set_one_param(cpe->core_handle,
				session, p_info, &epd_thres,
				LSM_ENDPOINT_DETECT_THRESHOLD);
	if (unlikely(rc))
		dev_err(rtd->dev,
			"%s: set_one_param(epd_threshold) failed, rc %d\n",
			__func__, rc);
done:
	return rc;
}

static int msm_cpe_lsm_set_mode(struct snd_pcm_substream *substream,
		struct lsm_params_info *p_info)
{
	struct snd_soc_pcm_runtime *rtd;
	struct cpe_lsm_data *lsm_d = NULL;
	struct cpe_priv *cpe = NULL;
	struct cpe_lsm_session *session = NULL;
	struct wcd_cpe_lsm_ops *lsm_ops;
	struct snd_lsm_detect_mode det_mode;
	int rc;

	if (!msm_cpe_lsm_is_valid_stream(substream, __func__))
		return -EINVAL;

	rtd = substream->private_data;
	lsm_d = cpe_get_lsm_data(substream);
	cpe = cpe_get_private_data(substream);
	session = lsm_d->lsm_session;
	lsm_ops = &cpe->lsm_ops;

	if (p_info->param_size != sizeof(det_mode)) {
		dev_err(rtd->dev,
			"%s: Invalid param_size %d\n",
			__func__, p_info->param_size);
		rc = -EINVAL;
		goto done;
	}

	if (copy_from_user(&det_mode, p_info->param_data,
			   p_info->param_size)) {
		dev_err(rtd->dev,
			"%s: copy_from_user failed, size = %d\n",
			__func__, p_info->param_size);
		rc = -EFAULT;
		goto done;
	}

	rc = lsm_ops->lsm_set_one_param(cpe->core_handle,
				session, p_info, &det_mode,
				LSM_OPERATION_MODE);
	if (unlikely(rc))
		dev_err(rtd->dev,
			"%s: set_one_param(epd_threshold) failed, rc %d\n",
			__func__, rc);
done:
	return rc;
}

static int msm_cpe_lsm_set_gain(struct snd_pcm_substream *substream,
		struct lsm_params_info *p_info)
{
	struct snd_soc_pcm_runtime *rtd;
	struct cpe_lsm_data *lsm_d = NULL;
	struct cpe_priv *cpe = NULL;
	struct cpe_lsm_session *session = NULL;
	struct wcd_cpe_lsm_ops *lsm_ops;
	struct snd_lsm_gain gain;
	int rc;

	if (!msm_cpe_lsm_is_valid_stream(substream, __func__))
		return -EINVAL;

	rtd = substream->private_data;
	lsm_d = cpe_get_lsm_data(substream);
	cpe = cpe_get_private_data(substream);
	session = lsm_d->lsm_session;
	lsm_ops = &cpe->lsm_ops;

	if (p_info->param_size != sizeof(gain)) {
		dev_err(rtd->dev,
			"%s: Invalid param_size %d\n",
			__func__, p_info->param_size);
		rc = -EINVAL;
		goto done;
	}

	if (copy_from_user(&gain, p_info->param_data,
			   p_info->param_size)) {
		dev_err(rtd->dev,
			"%s: copy_from_user failed, size = %d\n",
			__func__, p_info->param_size);
		rc = -EFAULT;
		goto done;
	}

	rc = lsm_ops->lsm_set_one_param(cpe->core_handle,
				session, p_info, &gain,
				LSM_GAIN);
	if (unlikely(rc))
		dev_err(rtd->dev,
			"%s: set_one_param(epd_threshold) failed, rc %d\n",
			__func__, rc);
done:
	return rc;

}

static int msm_cpe_lsm_set_conf(struct snd_pcm_substream *substream,
		struct lsm_params_info *p_info)
{
	struct snd_soc_pcm_runtime *rtd;
	struct cpe_lsm_data *lsm_d = NULL;
	struct cpe_priv *cpe = NULL;
	struct cpe_lsm_session *session = NULL;
	struct wcd_cpe_lsm_ops *lsm_ops;
	int rc;

	if (!msm_cpe_lsm_is_valid_stream(substream, __func__))
		return -EINVAL;

	rtd = substream->private_data;
	lsm_d = cpe_get_lsm_data(substream);
	cpe = cpe_get_private_data(substream);
	session = lsm_d->lsm_session;
	lsm_ops = &cpe->lsm_ops;

	session->num_confidence_levels =
			p_info->param_size;
	rc = msm_cpe_lsm_get_conf_levels(session,
			p_info->param_data);
	if (rc) {
		dev_err(rtd->dev,
			"%s: get_conf_levels failed, err = %d\n",
			__func__, rc);
		goto done;
	}

	rc = lsm_ops->lsm_set_one_param(cpe->core_handle,
				session, p_info, NULL,
				LSM_MIN_CONFIDENCE_LEVELS);
	if (unlikely(rc))
		dev_err(rtd->dev,
			"%s: set_one_param(conf_levels) failed, rc %d\n",
			__func__, rc);
done:
	return rc;
}

static int msm_cpe_lsm_reg_model(struct snd_pcm_substream *substream,
		struct lsm_params_info *p_info)
{
	struct snd_soc_pcm_runtime *rtd;
	struct cpe_lsm_data *lsm_d = NULL;
	struct cpe_priv *cpe = NULL;
	struct cpe_lsm_session *session = NULL;
	struct wcd_cpe_lsm_ops *lsm_ops;
	int rc;
	size_t offset;
	u8 *snd_model_ptr;

	if (!msm_cpe_lsm_is_valid_stream(substream, __func__))
		return -EINVAL;

	rtd = substream->private_data;
	lsm_d = cpe_get_lsm_data(substream);
	cpe = cpe_get_private_data(substream);
	session = lsm_d->lsm_session;
	lsm_ops = &cpe->lsm_ops;

	lsm_ops->lsm_get_snd_model_offset(cpe->core_handle,
			session, &offset);
	/* Check if 'p_info->param_size + offset' crosses U32_MAX. */
	if (p_info->param_size > U32_MAX - offset) {
		dev_err(rtd->dev,
			"%s: Invalid param_size %d\n",
			__func__, p_info->param_size);
		return -EINVAL;
	}
	session->snd_model_size = p_info->param_size + offset;

	session->snd_model_data = vzalloc(session->snd_model_size);
	if (!session->snd_model_data)
			return -ENOMEM;
	snd_model_ptr = ((u8 *) session->snd_model_data) + offset;

	if (copy_from_user(snd_model_ptr,
			   p_info->param_data, p_info->param_size)) {
		dev_err(rtd->dev,
			"%s: copy_from_user for snd_model failed\n",
			__func__);
		rc = -EFAULT;
		goto free_snd_model_data;
	}

	rc = lsm_ops->lsm_shmem_alloc(cpe->core_handle, session,
				      session->snd_model_size);
	if (rc != 0) {
		dev_err(rtd->dev,
			"%s: shared memory allocation failed, err = %d\n",
		       __func__, rc);
		rc = -EINVAL;
		goto free_snd_model_data;
	}

	rc = lsm_ops->lsm_set_one_param(cpe->core_handle,
				session, p_info, NULL,
				LSM_REG_SND_MODEL);
	if (unlikely(rc)) {
		dev_err(rtd->dev,
			"%s: set_one_param(snd_model) failed, rc %d\n",
			__func__, rc);
		goto dealloc_shmem;
	}
	return 0;

dealloc_shmem:
	lsm_ops->lsm_shmem_dealloc(cpe->core_handle, session);

free_snd_model_data:
	vfree(session->snd_model_data);
	return rc;
}

static int msm_cpe_lsm_dereg_model(struct snd_pcm_substream *substream,
		struct lsm_params_info *p_info)
{
	struct snd_soc_pcm_runtime *rtd;
	struct cpe_lsm_data *lsm_d = NULL;
	struct cpe_priv *cpe = NULL;
	struct cpe_lsm_session *session = NULL;
	struct wcd_cpe_lsm_ops *lsm_ops;
	int rc;

	if (!msm_cpe_lsm_is_valid_stream(substream, __func__))
		return -EINVAL;

	rtd = substream->private_data;
	lsm_d = cpe_get_lsm_data(substream);
	cpe = cpe_get_private_data(substream);
	session = lsm_d->lsm_session;
	lsm_ops = &cpe->lsm_ops;

	rc = lsm_ops->lsm_set_one_param(cpe->core_handle,
				session, p_info, NULL,
				LSM_DEREG_SND_MODEL);
	if (rc)
		dev_err(rtd->dev,
			"%s: dereg_snd_model failed\n",
			__func__);
	return lsm_ops->lsm_shmem_dealloc(cpe->core_handle, session);
}

static int msm_cpe_lsm_set_custom(struct snd_pcm_substream *substream,
		struct lsm_params_info *p_info)
{
	struct snd_soc_pcm_runtime *rtd;
	struct cpe_lsm_data *lsm_d = NULL;
	struct cpe_priv *cpe = NULL;
	struct cpe_lsm_session *session = NULL;
	struct wcd_cpe_lsm_ops *lsm_ops;
	u8 *data;
	int rc;

	if (!msm_cpe_lsm_is_valid_stream(substream, __func__))
		return -EINVAL;

	rtd = substream->private_data;
	lsm_d = cpe_get_lsm_data(substream);
	cpe = cpe_get_private_data(substream);
	session = lsm_d->lsm_session;
	lsm_ops = &cpe->lsm_ops;

	if (p_info->param_size > MSM_CPE_MAX_CUSTOM_PARAM_SIZE) {
		dev_err(rtd->dev,
			"%s: invalid size %d, max allowed %d\n",
			__func__, p_info->param_size,
			MSM_CPE_MAX_CUSTOM_PARAM_SIZE);
		return -EINVAL;
	}

	data = kzalloc(p_info->param_size, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	if (copy_from_user(data, p_info->param_data,
			   p_info->param_size)) {
		dev_err(rtd->dev,
			"%s: copy_from_user failed for custom params, size = %d\n",
			__func__, p_info->param_size);
		rc = -EFAULT;
		goto err_ret;
	}

	rc = lsm_ops->lsm_set_one_param(cpe->core_handle,
				session, p_info, data,
				LSM_CUSTOM_PARAMS);
	if (rc)
		dev_err(rtd->dev,
			"%s: custom_params failed, err = %d\n",
			__func__, rc);
err_ret:
	kfree(data);
	return rc;
}

static int msm_cpe_lsm_process_params(struct snd_pcm_substream *substream,
		struct snd_lsm_module_params *p_data,
		void *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct lsm_params_info *p_info;
	int i;
	int rc = 0;

	p_info = (struct lsm_params_info *) params;

	for (i = 0; i < p_data->num_params; i++) {
		dev_dbg(rtd->dev,
			"%s: param (%d), module_id = 0x%x, param_id = 0x%x, param_size = 0x%x, param_type = 0x%x\n",
			__func__, i, p_info->module_id,
			p_info->param_id, p_info->param_size,
			p_info->param_type);

		switch (p_info->param_type) {
		case LSM_ENDPOINT_DETECT_THRESHOLD:
			rc = msm_cpe_lsm_set_epd(substream, p_info);
			break;
		case LSM_OPERATION_MODE:
			rc = msm_cpe_lsm_set_mode(substream, p_info);
			break;
		case LSM_GAIN:
			rc = msm_cpe_lsm_set_gain(substream, p_info);
			break;
		case LSM_MIN_CONFIDENCE_LEVELS:
			rc = msm_cpe_lsm_set_conf(substream, p_info);
			break;
		case LSM_REG_SND_MODEL:
			rc = msm_cpe_lsm_reg_model(substream, p_info);
			break;
		case LSM_DEREG_SND_MODEL:
			rc = msm_cpe_lsm_dereg_model(substream, p_info);
			break;
		case LSM_CUSTOM_PARAMS:
			rc = msm_cpe_lsm_set_custom(substream, p_info);
			break;
		default:
			dev_err(rtd->dev,
				"%s: Invalid param_type %d\n",
				__func__, p_info->param_type);
			rc = -EINVAL;
			break;
		}
		if (rc) {
			pr_err("%s: set_param fail for param_type %d\n",
				__func__, p_info->param_type);
			return rc;
		}

		p_info++;
	}

	return rc;
}

static int msm_cpe_lsm_ioctl(struct snd_pcm_substream *substream,
			 unsigned int cmd, void *arg)
{
	int err = 0;
	struct snd_soc_pcm_runtime *rtd;
	struct cpe_priv *cpe = NULL;
	struct cpe_lsm_data *lsm_d = NULL;
	struct cpe_lsm_session *session = NULL;
	struct wcd_cpe_lsm_ops *lsm_ops;

	if (!substream || !substream->private_data) {
		pr_err("%s: invalid substream (%pK)\n",
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

	switch (cmd) {
	case SNDRV_LSM_REG_SND_MODEL_V2: {
		struct snd_lsm_sound_model_v2 snd_model;

		if (session->is_topology_used) {
			dev_err(rtd->dev,
				"%s: %s: not supported if using topology\n",
				__func__, "LSM_REG_SND_MODEL_V2");
			err = -EINVAL;
			goto done;
		}

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

		if (session->is_topology_used) {
			dev_err(rtd->dev,
				"%s: %s: not supported if using topology\n",
				__func__, "SNDRV_LSM_SET_PARAMS");
			err = -EINVAL;
			goto done;
		}

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

	case SNDRV_LSM_SET_MODULE_PARAMS: {
		struct snd_lsm_module_params p_data;
		size_t p_size;
		u8 *params;

		if (!session->is_topology_used) {
			dev_err(rtd->dev,
				"%s: %s: not supported if not using topology\n",
				__func__, "SET_MODULE_PARAMS");
			err = -EINVAL;
			goto done;
		}

		if (!arg) {
			dev_err(rtd->dev,
				"%s: %s: No Param data to set\n",
				__func__, "SET_MODULE_PARAMS");
			err = -EINVAL;
			goto done;
		}

		if (copy_from_user(&p_data, arg,
				   sizeof(p_data))) {
			dev_err(rtd->dev,
				"%s: %s: copy_from_user failed, size = %zd\n",
				__func__, "p_data", sizeof(p_data));
			err = -EFAULT;
			goto done;
		}

		if (p_data.num_params > LSM_PARAMS_MAX) {
			dev_err(rtd->dev,
				"%s: %s: Invalid num_params %d\n",
				__func__, "SET_MODULE_PARAMS",
				p_data.num_params);
			err = -EINVAL;
			goto done;
		}

		p_size = p_data.num_params *
			 sizeof(struct lsm_params_info);

		if (p_data.data_size != p_size) {
			dev_err(rtd->dev,
				"%s: %s: Invalid size %zd\n",
				__func__, "SET_MODULE_PARAMS", p_size);

			err = -EFAULT;
			goto done;
		}

		params = kzalloc(p_size, GFP_KERNEL);
		if (!params) {
			err = -ENOMEM;
			goto done;
		}

		if (copy_from_user(params, p_data.params,
				   p_data.data_size)) {
			dev_err(rtd->dev,
				"%s: %s: copy_from_user failed, size = %d\n",
				__func__, "params", p_data.data_size);
			kfree(params);
			err = -EFAULT;
			goto done;
		}

		err = msm_cpe_lsm_process_params(substream, &p_data, params);
		if (err)
			dev_err(rtd->dev,
				"%s: %s: Failed to set params, err = %d\n",
				__func__, "SET_MODULE_PARAMS", err);
		kfree(params);
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

#ifdef CONFIG_COMPAT
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

struct lsm_params_info_32 {
	u32 module_id;
	u32 param_id;
	u32 param_size;
	compat_uptr_t param_data;
	enum LSM_PARAM_TYPE param_type;
};

struct snd_lsm_module_params_32 {
	compat_uptr_t params;
	u32 num_params;
	u32 data_size;
};

enum {
	SNDRV_LSM_REG_SND_MODEL_V2_32 =
		_IOW('U', 0x07, struct snd_lsm_sound_model_v2_32),
	SNDRV_LSM_SET_PARAMS32 =
		_IOW('U', 0x0A, struct snd_lsm_detection_params_32),
	SNDRV_LSM_SET_MODULE_PARAMS_32 =
		_IOW('U', 0x0B, struct snd_lsm_module_params_32),
};

static int msm_cpe_lsm_ioctl_compat(struct snd_pcm_substream *substream,
			 unsigned int cmd, void *arg)
{
	int err = 0;
	struct snd_soc_pcm_runtime *rtd;
	struct cpe_priv *cpe = NULL;
	struct cpe_lsm_data *lsm_d = NULL;
	struct cpe_lsm_session *session = NULL;
	struct wcd_cpe_lsm_ops *lsm_ops;

	if (!substream || !substream->private_data) {
		pr_err("%s: invalid substream (%pK)\n",
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

	switch (cmd) {
	case SNDRV_LSM_REG_SND_MODEL_V2_32: {
		struct snd_lsm_sound_model_v2 snd_model;
		struct snd_lsm_sound_model_v2_32 snd_model32;

		if (session->is_topology_used) {
			dev_err(rtd->dev,
				"%s: %s: not supported if using topology\n",
				__func__, "LSM_REG_SND_MODEL_V2_32");
			err = -EINVAL;
			goto done;
		}

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
	case SNDRV_LSM_EVENT_STATUS: {
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

		if (session->is_topology_used) {
			dev_err(rtd->dev,
				"%s: %s: not supported if using topology\n",
				__func__, "SNDRV_LSM_SET_PARAMS32");

			err = -EINVAL;
			goto done;
		}

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

	case SNDRV_LSM_SET_MODULE_PARAMS_32: {
		struct snd_lsm_module_params_32 p_data_32;
		struct snd_lsm_module_params p_data;
		u8 *params, *params32;
		size_t p_size;
		struct lsm_params_info_32 *p_info_32;
		struct lsm_params_info *p_info;
		int i;

		if (!session->is_topology_used) {
			dev_err(rtd->dev,
				"%s: %s: not supported if not using topology\n",
				__func__, "SET_MODULE_PARAMS_32");
			err = -EINVAL;
			goto done;
		}

		if (copy_from_user(&p_data_32, arg,
				   sizeof(p_data_32))) {
			dev_err(rtd->dev,
				"%s: %s: copy_from_user failed, size = %zd\n",
				__func__, "SET_MODULE_PARAMS_32",
				sizeof(p_data_32));
			err = -EFAULT;
			goto done;
		}

		p_data.params = compat_ptr(p_data_32.params);
		p_data.num_params = p_data_32.num_params;
		p_data.data_size = p_data_32.data_size;

		if (p_data.num_params > LSM_PARAMS_MAX) {
			dev_err(rtd->dev,
				"%s: %s: Invalid num_params %d\n",
				__func__, "SET_MODULE_PARAMS_32",
				p_data.num_params);
			err = -EINVAL;
			goto done;
		}

		if (p_data.data_size !=
		    (p_data.num_params * sizeof(struct lsm_params_info_32))) {
			dev_err(rtd->dev,
				"%s: %s: Invalid size %d\n",
				__func__, "SET_MODULE_PARAMS_32",
				p_data.data_size);
			err = -EINVAL;
			goto done;
		}

		p_size = sizeof(struct lsm_params_info_32) *
			 p_data.num_params;

		params32 = kzalloc(p_size, GFP_KERNEL);
		if (!params32) {
			err = -ENOMEM;
			goto done;
		}

		p_size = sizeof(struct lsm_params_info) * p_data.num_params;
		params = kzalloc(p_size, GFP_KERNEL);
		if (!params) {
			kfree(params32);
			err = -ENOMEM;
			goto done;
		}

		if (copy_from_user(params32, p_data.params,
				   p_data.data_size)) {
			dev_err(rtd->dev,
				"%s: %s: copy_from_user failed, size = %d\n",
				__func__, "params32", p_data.data_size);
			kfree(params32);
			kfree(params);
			err = -EFAULT;
			goto done;
		}

		p_info_32 = (struct lsm_params_info_32 *) params32;
		p_info = (struct lsm_params_info *) params;
		for (i = 0; i < p_data.num_params; i++) {
			p_info->module_id = p_info_32->module_id;
			p_info->param_id = p_info_32->param_id;
			p_info->param_size = p_info_32->param_size;
			p_info->param_data = compat_ptr(p_info_32->param_data);
			p_info->param_type = p_info_32->param_type;

			p_info_32++;
			p_info++;
		}

		err = msm_cpe_lsm_process_params(substream,
					     &p_data, params);
		if (err)
			dev_err(rtd->dev,
				"%s: Failed to process params, err = %d\n",
				__func__, err);
		kfree(params);
		kfree(params32);
		break;
	}
	case SNDRV_LSM_REG_SND_MODEL_V2:
	case SNDRV_LSM_SET_PARAMS:
	case SNDRV_LSM_SET_MODULE_PARAMS:
		/*
		 * In ideal cases, the compat_ioctl should never be called
		 * with the above unlocked ioctl commands. Print error
		 * and return error if it does.
		 */
		dev_err(rtd->dev,
			"%s: Invalid cmd for compat_ioctl\n",
			__func__);
		err = -EINVAL;
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
	struct cpe_lsm_lab *lab_d = &lsm_d->lab;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_hw_params lsm_param;
	struct wcd_cpe_lsm_ops *lsm_ops;

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
	lab_d->pcm_size = snd_pcm_lib_buffer_bytes(substream);

	dev_dbg(rtd->dev,
		"%s: pcm_size 0x%x", __func__, lab_d->pcm_size);

	if (lsm_d->cpe_prepared) {
		dev_dbg(rtd->dev, "%s: CPE is alredy prepared\n",
			__func__);
		return 0;
	}

	lsm_ops = &cpe->lsm_ops;
	afe_ops = &cpe->afe_ops;
	afe_cfg = &(lsm_d->lsm_session->afe_port_cfg);

	switch (cpe->input_port_id) {
	case AFE_PORT_ID_3:
		afe_cfg->port_id = AFE_PORT_ID_3;
		afe_cfg->bit_width = 16;
		afe_cfg->num_channels = 1;
		afe_cfg->sample_rate = SAMPLE_RATE_48KHZ;
		rc = afe_ops->afe_port_cmd_cfg(cpe->core_handle, afe_cfg);
		break;
	case AFE_PORT_ID_1:
	default:
		afe_cfg->port_id = AFE_PORT_ID_1;
		afe_cfg->bit_width = 16;
		afe_cfg->num_channels = 1;
		afe_cfg->sample_rate = SAMPLE_RATE_16KHZ;
		rc = afe_ops->afe_set_params(cpe->core_handle,
					     afe_cfg, cpe->afe_mad_ctl);
		break;
	}

	if (rc != 0) {
		dev_err(rtd->dev,
			"%s: cpe afe params failed for port = %d, err = %d\n",
			 __func__, afe_cfg->port_id, rc);
		return rc;
	}
	lsm_param.sample_rate = afe_cfg->sample_rate;
	lsm_param.num_chs = afe_cfg->num_channels;
	lsm_param.bit_width = afe_cfg->bit_width;
	rc = lsm_ops->lsm_set_media_fmt_params(cpe->core_handle, lsm_session,
					       &lsm_param);
	if (rc)
		dev_dbg(rtd->dev,
			"%s: failed to set lsm media fmt params, err = %d\n",
			__func__, rc);

	/* Send connect to port (input) */
	rc = lsm_ops->lsm_set_port(cpe->core_handle, lsm_session,
				   &cpe->input_port_id);
	if (rc) {
		dev_err(rtd->dev,
			"%s: Failed to set connect input port, err=%d\n",
			__func__, rc);
		return rc;
	}

	if (cpe->input_port_id != 3) {
		rc = lsm_ops->lsm_get_afe_out_port_id(cpe->core_handle,
						      lsm_session);
		if (rc != 0) {
			dev_err(rtd->dev,
				"%s: failed to get port id, err = %d\n",
				__func__, rc);
			return rc;
		}
		/* Send connect to port (output) */
		rc = lsm_ops->lsm_set_port(cpe->core_handle, lsm_session,
					   &lsm_session->afe_out_port_id);
		if (rc) {
			dev_err(rtd->dev,
				"%s: Failed to set connect output port, err=%d\n",
				__func__, rc);
			return rc;
		}
	}
	rc = msm_cpe_afe_port_cntl(substream,
				   cpe->core_handle,
				   afe_ops, afe_cfg,
				   AFE_CMD_PORT_START);
	if (rc)
		dev_err(rtd->dev,
			"%s: cpe_afe_port start failed, err = %d\n",
			__func__, rc);
	else
		lsm_d->cpe_prepared = true;

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
	struct cpe_hw_params *hw_params = NULL;

	if (!cpe || !cpe->core_handle) {
		dev_err(rtd->dev,
			"%s: Invalid %s\n",
			__func__,
			(!cpe) ? "cpe" : "core");
		return -EINVAL;
	}

	if (!lsm_d || !lsm_d->lsm_session) {
		dev_err(rtd->dev,
			"%s: Invalid %s\n",
			__func__,
			(!lsm_d) ? "priv_data" : "session");
		return -EINVAL;
	}

	session = lsm_d->lsm_session;
	hw_params = &lsm_d->hw_params;
	hw_params->buf_sz = (params_buffer_bytes(params)
				/ params_periods(params));
	hw_params->period_count = params_periods(params);
	hw_params->channels = params_channels(params);
	hw_params->sample_rate = params_rate(params);

	if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE)
		hw_params->sample_size = 16;
	else if (params_format(params) ==
		 SNDRV_PCM_FORMAT_S24_LE)
		hw_params->sample_size = 24;
	else if (params_format(params) ==
		 SNDRV_PCM_FORMAT_S32_LE)
		hw_params->sample_size = 32;
	else {
		dev_err(rtd->dev,
			"%s: Invalid Format 0x%x\n",
			__func__, params_format(params));
		return -EINVAL;
	}

	dev_dbg(rtd->dev,
		"%s: Format %d buffer size(bytes) %d period count %d\n"
		" Channel %d period in bytes 0x%x Period Size 0x%x rate = %d\n",
		__func__, params_format(params), params_buffer_bytes(params),
		params_periods(params), params_channels(params),
		params_period_bytes(params), params_period_size(params),
		params_rate(params));

	return 0;
}

static snd_pcm_uframes_t msm_cpe_lsm_pointer(
				struct snd_pcm_substream *substream)
{

	struct cpe_lsm_data *lsm_d = cpe_get_lsm_data(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct cpe_lsm_session *session;
	struct cpe_lsm_lab *lab_d = &lsm_d->lab;

	session = lsm_d->lsm_session;
	if (lab_d->dma_write  >= lab_d->pcm_size)
		lab_d->dma_write = 0;
	dev_dbg(rtd->dev,
		"%s:pcm_dma_pos = %d\n",
		__func__, lab_d->dma_write);

	return bytes_to_frames(runtime, (lab_d->dma_write));
}

static int msm_cpe_lsm_copy(struct snd_pcm_substream *substream, int a,
	 snd_pcm_uframes_t hwoff, void __user *buf, snd_pcm_uframes_t frames)
{
	struct cpe_lsm_data *lsm_d = cpe_get_lsm_data(substream);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct cpe_lsm_session *session;
	struct cpe_lsm_lab *lab_d = &lsm_d->lab;
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

	/* Check if buffer reading is already in error state */
	if (lab_d->thread_status == MSM_LSM_LAB_THREAD_ERROR) {
		dev_err(rtd->dev,
			"%s: Bufferring is in error state\n",
			__func__);
		/*
		 * Advance the period so there is no wait in case
		 * read is invoked even after error is propogated
		 */
		atomic_inc(&lab_d->in_count);
		lab_d->dma_write += snd_pcm_lib_period_bytes(substream);
		snd_pcm_period_elapsed(substream);
		return -ENETRESET;
	} else if (lab_d->thread_status == MSM_LSM_LAB_THREAD_STOP) {
		dev_err(rtd->dev,
			"%s: Buferring is in stopped\n",
			__func__);
		return -EIO;
	}

	rc = wait_event_timeout(lab_d->period_wait,
			(atomic_read(&lab_d->in_count) ||
			atomic_read(&lab_d->abort_read)),
			(2 * HZ));
	if (atomic_read(&lab_d->abort_read)) {
		pr_debug("%s: LSM LAB Abort read\n", __func__);
		return -EIO;
	}
	if (lab_d->thread_status != MSM_LSM_LAB_THREAD_RUNNING) {
		pr_err("%s: Lab stopped\n", __func__);
		return -EIO;
	}
	if (!rc) {
		pr_err("%s:LAB err wait_event_timeout\n", __func__);
		rc = -EAGAIN;
		goto fail;
	}
	if (lab_d->buf_idx >= (lsm_d->hw_params.period_count))
		lab_d->buf_idx = 0;
	pcm_buf = (lab_d->pcm_buf[lab_d->buf_idx].mem);
	pr_debug("%s: Buf IDX = 0x%x pcm_buf %pK\n",
		 __func__,  lab_d->buf_idx, pcm_buf);
	if (pcm_buf) {
		if (copy_to_user(buf, pcm_buf, fbytes)) {
			pr_err("Failed to copy buf to user\n");
			rc = -EFAULT;
			goto fail;
		}
	}
	lab_d->buf_idx++;
	atomic_dec(&lab_d->in_count);
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
	const struct snd_kcontrol_new *kcontrol;
	bool found_runtime = false;
	const char *cpe_dev_id = "qcom,msm-cpe-lsm-id";
	u32 port_id = 0;
	int ret = 0;
	int i;

	if (!platform || !platform->component.card) {
		pr_err("%s: Invalid platform or card\n",
			__func__);
		return -EINVAL;
	}

	card = platform->component.card;

	/* Match platform to codec */
	for (i = 0; i < card->num_links; i++) {
		rtd = &card->rtd[i];
		if (!rtd->platform)
			continue;
		if (!strcmp(rtd->platform->component.name,
			    platform->component.name)) {
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

	ret = of_property_read_u32(platform->dev->of_node, cpe_dev_id,
				  &port_id);
	if (ret) {
		dev_dbg(platform->dev,
			"%s: missing 0x%x in dt node\n", __func__, port_id);
		port_id = 1;
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
	cpe_priv->input_port_id = port_id;
	wcd_cpe_get_lsm_ops(&cpe_priv->lsm_ops);
	wcd_cpe_get_afe_ops(&cpe_priv->afe_ops);

	snd_soc_platform_set_drvdata(platform, cpe_priv);
	kcontrol = &msm_cpe_kcontrols[0];
	snd_ctl_add(card->snd_card, snd_ctl_new1(kcontrol, cpe_priv));
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
