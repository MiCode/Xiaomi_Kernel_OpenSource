
/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/msm_audio.h>

#include <asm/atomic.h>
#include <mach/debug_mm.h>
#include <mach/qdsp6v2/audio_dev_ctl.h>
#include <sound/q6asm.h>
#include <sound/apr_audio.h>
#include <linux/wakelock.h>
#include <mach/cpuidle.h>

#define MAX_BUF 4

struct dma_buf {
	uint32_t addr;
	uint32_t v_addr;
	uint32_t used;
};
struct pcm {
	struct mutex lock;
	struct mutex read_lock;
	wait_queue_head_t wait;
	spinlock_t dsp_lock;
	struct audio_client *ac;
	uint32_t sample_rate;
	uint32_t channel_count;
	uint32_t buffer_size;
	uint32_t buffer_count;
	uint32_t cpu_idx;
	uint32_t dsp_idx;
	uint32_t start;
	uint32_t dma_addr;
	uint32_t dma_virt;
	struct dma_buf dma_buf[MAX_BUF];
	atomic_t in_count;
	atomic_t in_enabled;
	atomic_t in_opened;
	atomic_t in_stopped;
	int poll_time;
	struct hrtimer hrt;
};

static enum hrtimer_restart afe_hrtimer_callback(struct hrtimer *hrt);

static enum hrtimer_restart afe_hrtimer_callback(struct hrtimer *hrt)
{
	struct pcm *pcm =
		container_of(hrt, struct pcm, hrt);
	int rc = 0;
	if (pcm->start) {
		if (pcm->dsp_idx == pcm->buffer_count)
			pcm->dsp_idx = 0;
		if (pcm->dma_buf[pcm->dsp_idx].used == 0) {
			if (atomic_read(&pcm->in_stopped)) {
				pr_err("%s: Driver closed - return\n",
					__func__);
				return HRTIMER_NORESTART;
			}
			rc = afe_rt_proxy_port_read(
				pcm->dma_buf[pcm->dsp_idx].addr,
				pcm->buffer_size);
			if (rc < 0) {
				pr_err("%s afe_rt_proxy_port_read fail\n",
					__func__);
				goto fail;
			}
			pcm->dma_buf[pcm->dsp_idx].used = 1;
			pcm->dsp_idx++;
			pr_debug("sending frame rec to DSP: poll_time: %d\n",
					pcm->poll_time);
		} else {
			pr_err("Qcom: Used flag not reset retry after %d msec\n",
				(pcm->poll_time/10));
			goto fail_timer;
		}
fail:
		hrtimer_forward_now(hrt, ns_to_ktime(pcm->poll_time
				* 1000));
		return HRTIMER_RESTART;
fail_timer:
		hrtimer_forward_now(hrt, ns_to_ktime((pcm->poll_time/10)
				* 1000));

		return HRTIMER_RESTART;
	} else {
		return HRTIMER_NORESTART;
	}
}

static void pcm_afe_callback(uint32_t opcode,
		uint32_t token, uint32_t *payload,
		 void *priv)
{
	struct pcm *pcm = (struct pcm *)priv;
	unsigned long dsp_flags;
	uint16_t event;

	if (pcm == NULL)
		return;
	pr_debug("%s\n", __func__);
	spin_lock_irqsave(&pcm->dsp_lock, dsp_flags);
	switch (opcode) {
	case AFE_EVENT_RT_PROXY_PORT_STATUS: {
		event = (uint16_t)((0xFFFF0000 & payload[0]) >> 0x10);
		switch (event) {
		case AFE_EVENT_RTPORT_START: {
			pcm->dsp_idx = 0;
			pcm->cpu_idx = 0;
			pcm->poll_time = (unsigned long)
						(((pcm->buffer_size*1000)/
						(pcm->channel_count *
						pcm->sample_rate * 2))*1000);
			pr_debug("%s: poll_time:%d\n", __func__,
						pcm->poll_time);
			pcm->start = 1;
			wake_up(&pcm->wait);
			break;
		}
		case AFE_EVENT_RTPORT_STOP:
			pr_debug("%s: event!=0\n", __func__);
			pcm->start = 0;
			atomic_set(&pcm->in_stopped, 1);
			break;
		case AFE_EVENT_RTPORT_LOW_WM:
			pr_debug("%s: Underrun\n", __func__);
			break;
		case AFE_EVENT_RTPORT_HI_WM:
			pr_debug("%s: Overrun\n", __func__);
			break;
		default:
			break;
		}
		break;
	}
	case APR_BASIC_RSP_RESULT: {
		switch (payload[0]) {
		case AFE_SERVICE_CMD_RTPORT_RD:
			pr_debug("%s: Read done\n", __func__);
			atomic_inc(&pcm->in_count);
			wake_up(&pcm->wait);
			break;
		default:
			break;
		}
		break;
	}
	default:
		break;
	}
	spin_unlock_irqrestore(&pcm->dsp_lock, dsp_flags);
}

static uint32_t getbuffersize(uint32_t samplerate)
{
	if (samplerate == 8000)
		return 480*8;
	else if (samplerate == 16000)
		return 480*16;
	else if (samplerate == 48000)
		return 480*48;
	return 0;
}

static int pcm_in_open(struct inode *inode, struct file *file)
{
	struct pcm *pcm;
	int rc = 0;

	pr_debug("%s: pcm proxy in open session\n", __func__);
	pcm = kzalloc(sizeof(struct pcm), GFP_KERNEL);
	if (!pcm)
		return -ENOMEM;

	pcm->channel_count = 1;
	pcm->sample_rate = 8000;
	pcm->buffer_size = getbuffersize(pcm->sample_rate);
	pcm->buffer_count = MAX_BUF;

	pcm->ac = q6asm_audio_client_alloc(NULL, (void *)pcm);
	if (!pcm->ac) {
		pr_err("%s: Could not allocate memory\n", __func__);
		rc = -ENOMEM;
		goto fail;
	}

	mutex_init(&pcm->lock);
	mutex_init(&pcm->read_lock);
	spin_lock_init(&pcm->dsp_lock);
	init_waitqueue_head(&pcm->wait);

	hrtimer_init(&pcm->hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pcm->hrt.function = afe_hrtimer_callback;

	atomic_set(&pcm->in_stopped, 0);
	atomic_set(&pcm->in_enabled, 0);
	atomic_set(&pcm->in_count, 0);
	atomic_set(&pcm->in_opened, 1);

	file->private_data = pcm;
	pr_debug("%s: pcm proxy open success session id:%d\n",
				__func__, pcm->ac->session);
	return 0;
fail:
	if (pcm->ac)
		q6asm_audio_client_free(pcm->ac);
	kfree(pcm);
	return rc;
}

static int pcm_in_disable(struct pcm *pcm)
{
	int rc = 0;

	if (atomic_read(&pcm->in_opened)) {
		atomic_set(&pcm->in_enabled, 0);
		atomic_set(&pcm->in_opened, 0);
		atomic_set(&pcm->in_stopped, 1);
		wake_up(&pcm->wait);
	}
	return rc;
}

static int config(struct pcm *pcm)
{

	int ret = 0, i;
	struct audio_buffer *buf;

	pr_debug("%s\n", __func__);

	ret = q6asm_audio_client_buf_alloc_contiguous(OUT,
			pcm->ac,
			pcm->buffer_size,
			pcm->buffer_count);
	if (ret < 0) {
		pr_err("%s: Audio Start: Buffer Allocation failed rc = %d\n",
								__func__, ret);
		return -ENOMEM;
	}
	buf = pcm->ac->port[OUT].buf;

	if (buf == NULL || buf[0].data == NULL)
		return -ENOMEM;

	memset(buf[0].data, 0, pcm->buffer_size * pcm->buffer_count);
	pcm->dma_addr = (u32) buf[0].phys;
	pcm->dma_virt = (u32) buf[0].data;

	for (i = 0; i < pcm->buffer_count; i++) {
		pcm->dma_buf[i].addr = (u32) (buf[i].phys);
		pcm->dma_buf[i].v_addr = (u32) (buf[i].data);
		pcm->dma_buf[i].used = 0;
	}

	ret = afe_register_get_events(RT_PROXY_DAI_001_TX,
			pcm_afe_callback, pcm);
	if (ret < 0) {
		pr_err("%s: afe-pcm:register for events failed\n", __func__);
		return ret;
	}
	ret = afe_cmd_memory_map(pcm->dma_addr,
			pcm->buffer_size * pcm->buffer_count);
	if (ret < 0) {
		pr_err("%s: fail to map memory to DSP\n", __func__);
		return ret;
	}

	pr_debug("%s:success\n", __func__);
	return ret;
}
static bool is_dma_buf_avail(struct pcm *pcm)
{
	return (pcm->dma_buf[pcm->cpu_idx].used == 1);
}
static ssize_t pcm_in_read(struct file *file, char __user *buf,
			  size_t count, loff_t *pos)
{
	struct pcm *pcm = file->private_data;
	const char __user *start = buf;
	int rc = 0;
	bool rc1 = false;
	int len = 0;

	if (!atomic_read(&pcm->in_enabled))
		return -EFAULT;
	mutex_lock(&pcm->read_lock);
	while (count > 0) {
		rc = wait_event_timeout(pcm->wait,
				(atomic_read(&pcm->in_count) ||
				atomic_read(&pcm->in_stopped)), 2 * HZ);
		if (!rc) {
			pr_err("%s: wait_event_timeout failed\n", __func__);
			goto fail;
		}
		if (atomic_read(&pcm->in_stopped) &&
					!atomic_read(&pcm->in_count)) {
			pr_err("%s: count:%d/stopped:%d failed\n", __func__,
					atomic_read(&pcm->in_count),
					atomic_read(&pcm->in_stopped));
			mutex_unlock(&pcm->read_lock);
			return 0;
		}

		rc1 = is_dma_buf_avail(pcm);
		if (!rc1) {
			pr_err("%s: DMA buf not ready-returning from read\n",
								__func__);
			goto fail;
		}
		if (count >= pcm->buffer_size)
			len = pcm->buffer_size;
		else {
			len = count;
			pr_err("%s: short bytesavail[%d]"\
				"bytesrequest[%d]"\
				"bytesrejected%d]\n",\
				__func__, pcm->buffer_size,
				count, (pcm->buffer_size - count));
		}
		if (len) {
			if (copy_to_user(buf,
				(char *)(pcm->dma_buf[pcm->cpu_idx].v_addr),
				len)) {
				pr_err("%s copy_to_user failed len[%d]\n",
							__func__, len);
				rc = -EFAULT;
				goto fail;
			}
			count -= len;
			buf += len;
		}
		atomic_dec(&pcm->in_count);
		memset((char *)(pcm->dma_buf[pcm->cpu_idx].v_addr),
						0, pcm->buffer_size);
		pcm->dma_buf[pcm->cpu_idx].used = 0;
		wake_up(&pcm->wait);
		pcm->cpu_idx++;
		if (pcm->cpu_idx == pcm->buffer_count)
			pcm->cpu_idx = 0;

	}
	rc = buf-start;
	pr_debug("%s: pcm_in_read:rc:%d\n", __func__, rc);

fail:
	mutex_unlock(&pcm->read_lock);
	return rc;
}

static int afe_start(struct pcm *pcm)
{
	union afe_port_config port_config;
	port_config.rtproxy.num_ch =
			pcm->channel_count;

	pr_debug("%s: channel %d entered,port: %d,rate: %d\n", __func__,
	port_config.rtproxy.num_ch, RT_PROXY_DAI_001_TX, pcm->sample_rate);

	port_config.rtproxy.bitwidth = 16; /* Q6 only supports 16 */
	port_config.rtproxy.interleaved = 1;
	port_config.rtproxy.frame_sz = pcm->buffer_size;
	port_config.rtproxy.jitter =
				port_config.rtproxy.frame_sz/2;
	port_config.rtproxy.lw_mark = 0;
	port_config.rtproxy.hw_mark = 0;
	port_config.rtproxy.rsvd = 0;
	afe_open(RT_PROXY_DAI_001_TX, &port_config, pcm->sample_rate);
	return 0;

}

static long pcm_in_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct pcm *pcm = file->private_data;
	int rc = 0;

	mutex_lock(&pcm->lock);
	switch (cmd) {
	case AUDIO_START: {
		pr_debug("%s: AUDIO_START\n", __func__);
		if (atomic_read(&pcm->in_enabled)) {
			pr_info("%s:AUDIO_START already over\n", __func__);
			rc = 0;
			break;
		}
		rc = config(pcm);
		if (rc) {
			pr_err("%s: IN Configuration failed\n", __func__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: call config done\n", __func__);
		atomic_set(&pcm->in_enabled, 1);
		afe_start(pcm);
		rc = wait_event_timeout(pcm->wait,
				((pcm->start == 1) ||
				atomic_read(&pcm->in_stopped)), 5 * HZ);
		if (!rc) {
			pr_err("%s: wait_event_timeout failed\n", __func__);
			goto fail;
		}
		pr_debug("%s: afe start done\n", __func__);
		if (atomic_read(&pcm->in_stopped)) {
			pr_err("%s: stopped unexpected before start!!\n",
								__func__);
			mutex_unlock(&pcm->lock);
			return 0;
		}

		hrtimer_start(&pcm->hrt, ns_to_ktime(0),
					HRTIMER_MODE_REL);
		break;
	}
	case AUDIO_STOP:
		break;
	case AUDIO_FLUSH:
		break;
	case AUDIO_SET_CONFIG: {
		struct msm_audio_config config;

		if (copy_from_user(&config, (void *) arg, sizeof(config))) {
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: SET_CONFIG: channel_count:%d"\
			"sample_rate:%d\n", __func__,
			config.channel_count,
			config.sample_rate);

		if (!config.channel_count || config.channel_count > 2) {
			pr_err("%s: Channels(%d) not supported\n",
				__func__, config.channel_count);
			rc = -EINVAL;
			break;
		}

		if (config.sample_rate != 8000 &&
			config.sample_rate != 16000 &&
			config.sample_rate != 48000) {
			pr_err("%s: Sample rate(%d) not supported\n",
				__func__, config.sample_rate);
			rc = -EINVAL;
			break;
		}

		pcm->sample_rate = config.sample_rate;
		pcm->channel_count = config.channel_count;
		pcm->buffer_size = getbuffersize(pcm->sample_rate);

		pr_debug("%s: Calculated buff size %d", __func__,
						pcm->buffer_size);
		break;
	}
	case AUDIO_GET_CONFIG: {
		struct msm_audio_config config;
		config.buffer_size = pcm->buffer_size;
		config.buffer_count = pcm->buffer_count;
		config.sample_rate = pcm->sample_rate;
		config.channel_count = pcm->channel_count;
		config.unused[0] = 0;
		config.unused[1] = 0;
		config.unused[2] = 0;
		if (copy_to_user((void *) arg, &config, sizeof(config)))
			rc = -EFAULT;
		break;
	}
	case AUDIO_PAUSE:
		pr_debug("%s: AUDIO_PAUSE %ld\n", __func__, arg);
		if (arg == 1) {
			pcm->start = 0;
		} else if (arg == 0) {
			pcm->start = 1;
			hrtimer_start(&pcm->hrt, ns_to_ktime(0),
					HRTIMER_MODE_REL);
		}
	break;

	default:
		rc = -EINVAL;
		break;
	}
fail:
	mutex_unlock(&pcm->lock);
	return rc;
}

static int pcm_in_release(struct inode *inode, struct file *file)
{
	int rc = 0;
	struct pcm *pcm = file->private_data;

	pr_debug("[%s:%s] release session id[%d]\n", __MM_FILE__,
		__func__, pcm->ac->session);
	mutex_lock(&pcm->lock);


	/* remove this session from topology list */
	auddev_cfg_tx_copp_topology(pcm->ac->session,
				DEFAULT_COPP_TOPOLOGY);

	rc = pcm_in_disable(pcm);
	hrtimer_cancel(&pcm->hrt);
	rc = afe_cmd_memory_unmap(pcm->dma_addr);
	if (rc < 0)
		pr_err("%s: AFE memory unmap failed\n", __func__);
	rc =  afe_unregister_get_events(RT_PROXY_DAI_001_TX);
	if (rc < 0)
		pr_err("%s: AFE unregister for events failed\n", __func__);

	afe_close(RT_PROXY_DAI_001_TX);
	pr_debug("%s: release all buffer\n", __func__);
	q6asm_audio_client_buf_free_contiguous(OUT,
				pcm->ac);
	msm_clear_session_id(pcm->ac->session);
	q6asm_audio_client_free(pcm->ac);
	mutex_unlock(&pcm->lock);
	mutex_destroy(&pcm->lock);
	mutex_destroy(&pcm->read_lock);
	kfree(pcm);
	return rc;
}

static const struct file_operations pcm_in_proxy_fops = {
	.owner		= THIS_MODULE,
	.open		= pcm_in_open,
	.read		= pcm_in_read,
	.release	= pcm_in_release,
	.unlocked_ioctl	= pcm_in_ioctl,
};

struct miscdevice pcm_in_proxy_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_pcm_in_proxy",
	.fops	= &pcm_in_proxy_fops,
};

static int snddev_rtproxy_open(struct msm_snddev_info *dev_info)
{
	return 0;
}

static int snddev_rtproxy_close(struct msm_snddev_info *dev_info)
{
	return 0;
}

static int snddev_rtproxy_set_freq(struct msm_snddev_info *dev_info,
				u32 req_freq)
{
	return 48000;
}

static int __init pcm_in_proxy_init(void)
{
	struct msm_snddev_info *dev_info;

	dev_info = kzalloc(sizeof(struct msm_snddev_info), GFP_KERNEL);
	if (!dev_info) {
		pr_err("unable to allocate memeory for msm_snddev_info\n");
		return -ENOMEM;
	}
	dev_info->name = "rtproxy_rx";
	dev_info->copp_id = RT_PROXY_PORT_001_RX;
	dev_info->acdb_id = 0;
	dev_info->private_data = NULL;
	dev_info->dev_ops.open = snddev_rtproxy_open;
	dev_info->dev_ops.close = snddev_rtproxy_close;
	dev_info->dev_ops.set_freq = snddev_rtproxy_set_freq;
	dev_info->capability = SNDDEV_CAP_RX;
	dev_info->opened = 0;
	msm_snddev_register(dev_info);
	dev_info->sample_rate = 48000;

	pr_debug("%s: init done for proxy\n", __func__);

	return misc_register(&pcm_in_proxy_misc);
}

device_initcall(pcm_in_proxy_init);
