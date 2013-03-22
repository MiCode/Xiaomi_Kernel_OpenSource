/*
 * pcm audio output device
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2009-2012, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <asm/atomic.h>
#include <asm/ioctls.h>

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/memory_alloc.h>
#include <linux/msm_audio.h>
#include <linux/pm_qos.h>

#include <mach/msm_adsp.h>
#include <mach/iommu.h>
#include <mach/iommu_domains.h>
#include <mach/qdsp5v2/qdsp5audppcmdi.h>
#include <mach/qdsp5v2/qdsp5audppmsg.h>
#include <mach/qdsp5v2/audio_dev_ctl.h>
#include <mach/qdsp5v2/audpp.h>
#include <mach/qdsp5v2/audio_dev_ctl.h>
#include <mach/msm_memtypes.h>
#include <mach/cpuidle.h>
#include <linux/msm_ion.h>
#include <mach/htc_pwrsink.h>
#include <mach/debug_mm.h>

#define BUFSZ (960 * 5)
#define DMASZ (BUFSZ * 2)

#define HOSTPCM_STREAM_ID 5

struct buffer {
	void *data;
	unsigned size;
	unsigned used;
	unsigned addr;
};

struct audio {
	struct buffer out[2];

	spinlock_t dsp_lock;

	uint8_t out_head;
	uint8_t out_tail;
	uint8_t out_needed; /* number of buffers the dsp is waiting for */

	atomic_t out_bytes;

	struct mutex lock;
	struct mutex write_lock;
	wait_queue_head_t wait;

	/* configuration to use on next enable */
	uint32_t out_sample_rate;
	uint32_t out_channel_mode;
	uint32_t out_weight;
	uint32_t out_buffer_size;
	uint32_t device_events;
	int16_t source;

	/* data allocated for various buffers */
	char *data;
	dma_addr_t phys;
	void *map_v_write;
	int teos; /* valid only if tunnel mode & no data left for decoder */
	int opened;
	int enabled;
	int running;
	int stopped; /* set when stopped, cleared on flush */
	uint16_t dec_id;
	int voice_state;

	struct wake_lock wakelock;
	struct pm_qos_request pm_qos_req;

	struct audpp_cmd_cfg_object_params_volume vol_pan;
	struct ion_client *client;
	struct ion_handle *buff_handle;
};

static void audio_out_listener(u32 evt_id, union auddev_evt_data *evt_payload,
				void *private_data)
{
	struct audio *audio = private_data;
	switch (evt_id) {
	case AUDDEV_EVT_DEV_RDY:
		MM_DBG(":AUDDEV_EVT_DEV_RDY\n");
		audio->source |= (0x1 << evt_payload->routing_id);
		if (audio->running == 1 && audio->enabled == 1)
			audpp_route_stream(audio->dec_id, audio->source);
		break;
	case AUDDEV_EVT_DEV_RLS:
		MM_DBG(":AUDDEV_EVT_DEV_RLS\n");
		audio->source &= ~(0x1 << evt_payload->routing_id);
		if (audio->running == 1 && audio->enabled == 1)
			audpp_route_stream(audio->dec_id, audio->source);
		break;
	case AUDDEV_EVT_STREAM_VOL_CHG:
		audio->vol_pan.volume = evt_payload->session_vol;
		MM_DBG(":AUDDEV_EVT_STREAM_VOL_CHG, stream vol %d\n",
				audio->vol_pan.volume);
		if (audio->running)
			audpp_dsp_set_vol_pan(audio->dec_id, &audio->vol_pan,
					POPP);
		break;
	case AUDDEV_EVT_VOICE_STATE_CHG:
		MM_DBG("AUDDEV_EVT_VOICE_STATE_CHG, state = %d\n",
						evt_payload->voice_state);
		audio->voice_state = evt_payload->voice_state;
		/* Voice uplink Rx case */
		if (audio->running &&
			(audio->source & AUDPP_MIXER_UPLINK_RX) &&
			(audio->voice_state == VOICE_STATE_OFFCALL)) {
			MM_DBG("Voice is terminated, Wake up write: %x %x\n",
					audio->voice_state, audio->source);
			wake_up(&audio->wait);
		}
		break;
	default:
		MM_ERR("ERROR:wrong event\n");
		break;
       }
}

static void audio_prevent_sleep(struct audio *audio)
{
	MM_DBG("\n"); /* Macro prints the file name and function */
	wake_lock(&audio->wakelock);
	pm_qos_update_request(&audio->pm_qos_req,
			      msm_cpuidle_get_deep_idle_latency());
}

static void audio_allow_sleep(struct audio *audio)
{
	pm_qos_update_request(&audio->pm_qos_req, PM_QOS_DEFAULT_VALUE);
	wake_unlock(&audio->wakelock);
	MM_DBG("\n"); /* Macro prints the file name and function */
}

static int audio_dsp_out_enable(struct audio *audio, int yes);
static int audio_dsp_send_buffer(struct audio *audio, unsigned id,
	unsigned len);

static void audio_dsp_event(void *private, unsigned id, uint16_t *msg);

/* must be called with audio->lock held */
static int audio_enable(struct audio *audio)
{
	MM_DBG("\n"); /* Macro prints the file name and function */

	if (audio->enabled)
		return 0;

	/* refuse to start if we're not ready */
	if (!audio->out[0].used || !audio->out[1].used)
		return -EIO;

	/* we start buffers 0 and 1, so buffer 0 will be the
	 * next one the dsp will want
	 */
	audio->out_tail = 0;
	audio->out_needed = 0;

	audio_prevent_sleep(audio);

	if (audpp_enable(-1, audio_dsp_event, audio)) {
		MM_ERR("audpp_enable() failed\n");
		audio_allow_sleep(audio);
		return -ENODEV;
	}

	audio->enabled = 1;
	htc_pwrsink_set(PWRSINK_AUDIO, 100);
	return 0;
}

/* must be called with audio->lock held */
static int audio_disable(struct audio *audio)
{
	MM_DBG("\n"); /* Macro prints the file name and function */
	if (audio->enabled) {
		audio->enabled = 0;
		audio_dsp_out_enable(audio, 0);

		audpp_disable(-1, audio);

		wake_up(&audio->wait);
		audio->out_needed = 0;
		audio_allow_sleep(audio);
	}
	return 0;
}

/* ------------------- dsp --------------------- */
static void audio_dsp_event(void *private, unsigned id, uint16_t *msg)
{
	struct audio *audio = private;
	struct buffer *frame;
	unsigned long flags;
	static unsigned long pcmdmamsd_time;

	switch (id) {
	case AUDPP_MSG_HOST_PCM_INTF_MSG: {
		unsigned id = msg[3];
		unsigned idx = msg[4] - 1;

		MM_DBG("HOST_PCM id %d idx %d\n", id, idx);
		if (id != AUDPP_MSG_HOSTPCM_ID_ARM_RX) {
			MM_ERR("bogus id\n");
			break;
		}
		if (idx > 1) {
			MM_ERR("bogus buffer idx\n");
			break;
		}
		spin_lock_irqsave(&audio->dsp_lock, flags);
		if (audio->running) {
			atomic_add(audio->out[idx].used, &audio->out_bytes);
			audio->out[idx].used = 0;
			frame = audio->out + audio->out_tail;
			if (frame->used) {
				/* Reset teos flag to avoid stale
				 * PCMDMAMISS been considered
				 */
				audio->teos = 0;
				audio_dsp_send_buffer(
					audio, audio->out_tail, frame->used);
				audio->out_tail ^= 1;
			} else {
				audio->out_needed++;
			}
			wake_up(&audio->wait);
		}
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		break;
	}
	case AUDPP_MSG_PCMDMAMISSED:
		/* prints only if 1 second is elapsed since the last time
		 * this message has been printed */
		if (printk_timed_ratelimit(&pcmdmamsd_time, 1000))
			printk(KERN_INFO "[%s:%s] PCMDMAMISSED %d\n",
					__MM_FILE__, __func__, msg[0]);
		audio->teos++;
		MM_DBG("PCMDMAMISSED Count per Buffer %d\n", audio->teos);
		wake_up(&audio->wait);
		break;
	case AUDPP_MSG_CFG_MSG:
		if (msg[0] == AUDPP_MSG_ENA_ENA) {
			MM_DBG("CFG_MSG ENABLE\n");
			audio->out_needed = 0;
			audio->running = 1;
			audpp_dsp_set_vol_pan(audio->dec_id, &audio->vol_pan,
					POPP);
			audpp_route_stream(audio->dec_id, audio->source);
			audio_dsp_out_enable(audio, 1);
		} else if (msg[0] == AUDPP_MSG_ENA_DIS) {
			MM_DBG("CFG_MSG DISABLE\n");
			audio->running = 0;
		} else {
			MM_ERR("CFG_MSG %d?\n", msg[0]);
		}
		break;
	default:
		MM_ERR("UNKNOWN (%d)\n", id);
	}
}

static int audio_dsp_out_enable(struct audio *audio, int yes)
{
	struct audpp_cmd_pcm_intf cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id	= AUDPP_CMD_PCM_INTF;
	cmd.stream	= AUDPP_CMD_POPP_STREAM;
	cmd.stream_id	= audio->dec_id;
	cmd.config	= AUDPP_CMD_PCM_INTF_CONFIG_CMD_V;
	cmd.intf_type	= AUDPP_CMD_PCM_INTF_RX_ENA_ARMTODSP_V;

	if (yes) {
		cmd.write_buf1LSW	= audio->out[0].addr;
		cmd.write_buf1MSW	= audio->out[0].addr >> 16;
		if (audio->out[0].used)
			cmd.write_buf1_len	= audio->out[0].used;
		else
			cmd.write_buf1_len	= audio->out[0].size;
		cmd.write_buf2LSW	= audio->out[1].addr;
		cmd.write_buf2MSW	= audio->out[1].addr >> 16;
		if (audio->out[1].used)
			cmd.write_buf2_len	= audio->out[1].used;
		else
			cmd.write_buf2_len	= audio->out[1].size;
		cmd.arm_to_rx_flag	= AUDPP_CMD_PCM_INTF_ENA_V;
		cmd.weight_decoder_to_rx = audio->out_weight;
		cmd.weight_arm_to_rx	= 1;
		cmd.partition_number_arm_to_dsp = 0;
		cmd.sample_rate		= audio->out_sample_rate;
		cmd.channel_mode	= audio->out_channel_mode;
	}

	return audpp_send_queue2(&cmd, sizeof(cmd));
}

static int audio_dsp_send_buffer(struct audio *audio, unsigned idx,
	unsigned len)
{
	struct audpp_cmd_pcm_intf_send_buffer cmd;

	cmd.cmd_id	= AUDPP_CMD_PCM_INTF;
	cmd.stream	= AUDPP_CMD_POPP_STREAM;
	cmd.stream_id	= audio->dec_id;
	cmd.config	= AUDPP_CMD_PCM_INTF_BUFFER_CMD_V;
	cmd.intf_type	= AUDPP_CMD_PCM_INTF_RX_ENA_ARMTODSP_V;
	cmd.dsp_to_arm_buf_id	= 0;
	cmd.arm_to_dsp_buf_id	= idx + 1;
	cmd.arm_to_dsp_buf_len	= len;

	return audpp_send_queue2(&cmd, sizeof(cmd));
}

/* ------------------- device --------------------- */
static void audio_flush(struct audio *audio)
{
	audio->out[0].used = 0;
	audio->out[1].used = 0;
	audio->out_head = 0;
	audio->out_tail = 0;
	audio->stopped = 0;
}

static long audio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct audio *audio = file->private_data;
	int rc = -EINVAL;
	unsigned long flags = 0;

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;
		stats.byte_count = atomic_read(&audio->out_bytes);
		if (copy_to_user((void *) arg, &stats, sizeof(stats)))
			return -EFAULT;
		return 0;
	}

	switch (cmd) {
	case AUDIO_SET_VOLUME:
		spin_lock_irqsave(&audio->dsp_lock, flags);
		audio->vol_pan.volume = arg;
		if (audio->running)
			audpp_dsp_set_vol_pan(audio->dec_id, &audio->vol_pan,
					POPP);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		return 0;

	case AUDIO_SET_PAN:
		spin_lock_irqsave(&audio->dsp_lock, flags);
		audio->vol_pan.pan = arg;
		if (audio->running)
			audpp_dsp_set_vol_pan(audio->dec_id, &audio->vol_pan,
					POPP);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		return 0;
	}

	mutex_lock(&audio->lock);
	switch (cmd) {
	case AUDIO_START:
		if ((audio->voice_state != VOICE_STATE_INCALL)
			&& (audio->source & AUDPP_MIXER_UPLINK_RX)) {
			MM_ERR("Unable to Start : state %d source %d\n",
					audio->voice_state, audio->source);
			rc = -EPERM;
			break;
		}
		rc = audio_enable(audio);
		break;
	case AUDIO_STOP:
		rc = audio_disable(audio);
		audio->stopped = 1;
		break;
	case AUDIO_FLUSH:
		if (audio->stopped) {
			/* Make sure we're stopped and we wake any threads
			 * that might be blocked holding the write_lock.
			 * While audio->stopped write threads will always
			 * exit immediately.
			 */
			wake_up(&audio->wait);
			mutex_lock(&audio->write_lock);
			audio_flush(audio);
			mutex_unlock(&audio->write_lock);
		}
	case AUDIO_SET_CONFIG: {
		struct msm_audio_config config;
		if (copy_from_user(&config, (void *) arg, sizeof(config))) {
			rc = -EFAULT;
			break;
		}
		if (config.channel_count == 1)
			config.channel_count = AUDPP_CMD_PCM_INTF_MONO_V;
		else if (config.channel_count == 2)
			config.channel_count = AUDPP_CMD_PCM_INTF_STEREO_V;
		else {
			rc = -EINVAL;
			break;
		}
		audio->out_sample_rate = config.sample_rate;
		audio->out_channel_mode = config.channel_count;
		rc = 0;
		break;
	}
	case AUDIO_GET_CONFIG: {
		struct msm_audio_config config;
		config.buffer_size = BUFSZ;
		config.buffer_count = 2;
		config.sample_rate = audio->out_sample_rate;
		if (audio->out_channel_mode == AUDPP_CMD_PCM_INTF_MONO_V)
			config.channel_count = 1;
		else
			config.channel_count = 2;

		config.unused[0] = 0;
		config.unused[1] = 0;
		config.unused[2] = 0;
		if (copy_to_user((void *) arg, &config, sizeof(config)))
			rc = -EFAULT;
		else
			rc = 0;
		break;
	}
	case AUDIO_GET_SESSION_ID: {
		if (copy_to_user((void *) arg, &audio->dec_id,
					sizeof(unsigned short)))
			return -EFAULT;
		rc = 0;
		break;
	}
	default:
		rc = -EINVAL;
	}
	mutex_unlock(&audio->lock);
	return rc;
}

/* Only useful in tunnel-mode */
static int audio_fsync(struct file *file, loff_t ppos1, loff_t ppos2, int datasync)
{
	struct audio *audio = file->private_data;
	int rc = 0;

	if (!audio->running)
		return -EINVAL;

	mutex_lock(&audio->write_lock);

	/* PCM DMAMISS message is sent only once in
	 * hpcm interface. So, wait for buffer complete
	 * and teos flag.
	 */
	rc = wait_event_interruptible(audio->wait,
		(!audio->out[0].used &&
		!audio->out[1].used));

	if (rc < 0)
		goto done;

	rc = wait_event_interruptible(audio->wait,
		audio->teos);
done:
	mutex_unlock(&audio->write_lock);
	return rc;
}

static ssize_t audio_read(struct file *file, char __user *buf,
		size_t count, loff_t *pos)
{
	return -EINVAL;
}

static inline int rt_policy(int policy)
{
	if (unlikely(policy == SCHED_FIFO) || unlikely(policy == SCHED_RR))
		return 1;
	return 0;
}

static inline int task_has_rt_policy(struct task_struct *p)
{
	return rt_policy(p->policy);
}

static ssize_t audio_write(struct file *file, const char __user *buf,
			   size_t count, loff_t *pos)
{
	struct sched_param s = { .sched_priority = 1 };
	struct audio *audio = file->private_data;
	unsigned long flags;
	const char __user *start = buf;
	struct buffer *frame;
	size_t xfer;
	int old_prio = current->rt_priority;
	int old_policy = current->policy;
	int cap_nice = cap_raised(current_cap(), CAP_SYS_NICE);
	int rc = 0;


	if ((audio->voice_state == VOICE_STATE_OFFCALL)
		&& (audio->source & AUDPP_MIXER_UPLINK_RX) &&
							audio->running) {
		MM_ERR("Not Permitted Voice Terminated: state %d source %x \
			running %d\n",
			audio->voice_state, audio->source, audio->running);
		return -EPERM;
	}
	/* just for this write, set us real-time */
	if (!task_has_rt_policy(current)) {
		struct cred *new = prepare_creds();
		cap_raise(new->cap_effective, CAP_SYS_NICE);
		commit_creds(new);
		if ((sched_setscheduler(current, SCHED_RR, &s)) < 0)
			MM_ERR("sched_setscheduler failed\n");
	}

	mutex_lock(&audio->write_lock);
	while (count > 0) {
		frame = audio->out + audio->out_head;

		rc = wait_event_interruptible(audio->wait,
		      (frame->used == 0) || (audio->stopped) ||
			((audio->voice_state == VOICE_STATE_OFFCALL) &&
			(audio->source & AUDPP_MIXER_UPLINK_RX)));

		if (rc < 0)
			break;
		if (audio->stopped) {
			rc = -EBUSY;
			break;
		} else if ((audio->voice_state == VOICE_STATE_OFFCALL) &&
			(audio->source & AUDPP_MIXER_UPLINK_RX)) {
			MM_ERR("Not Permitted Voice Terminated: %d\n",
							audio->voice_state);
			rc = -EPERM;
			break;
		}

		xfer = count > frame->size ? frame->size : count;
		if (copy_from_user(frame->data, buf, xfer)) {
			rc = -EFAULT;
			break;
		}
		frame->used = xfer;
		audio->out_head ^= 1;
		count -= xfer;
		buf += xfer;

		spin_lock_irqsave(&audio->dsp_lock, flags);
		frame = audio->out + audio->out_tail;
		if (frame->used && audio->out_needed) {
			/* Reset teos flag to avoid stale
			 * PCMDMAMISS been considered
			 */
			audio->teos = 0;
			audio_dsp_send_buffer(audio, audio->out_tail,
					frame->used);
			audio->out_tail ^= 1;
			audio->out_needed--;
		}
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
	}

	mutex_unlock(&audio->write_lock);

	/* restore scheduling policy and priority */
	if (!rt_policy(old_policy)) {
		struct sched_param v = { .sched_priority = old_prio };
		if ((sched_setscheduler(current, old_policy, &v)) < 0)
			MM_ERR("sched_setscheduler failed\n");
		if (likely(!cap_nice)) {
			struct cred *new = prepare_creds();
			cap_lower(new->cap_effective, CAP_SYS_NICE);
			commit_creds(new);
		}
	}

	if (buf > start)
		return buf - start;
	return rc;
}

static int audio_release(struct inode *inode, struct file *file)
{
	struct audio *audio = file->private_data;

	mutex_lock(&audio->lock);
	auddev_unregister_evt_listner(AUDDEV_CLNT_DEC, audio->dec_id);
	audio_disable(audio);
	audio_flush(audio);
	audio->opened = 0;
	mutex_unlock(&audio->lock);
	htc_pwrsink_set(PWRSINK_AUDIO, 0);
	return 0;
}

static struct audio the_audio;

static int audio_open(struct inode *inode, struct file *file)
{
	struct audio *audio = &the_audio;
	int rc;

	mutex_lock(&audio->lock);

	if (audio->opened) {
		MM_ERR("busy\n");
		rc = -EBUSY;
		goto done;
	}


	audio->dec_id = HOSTPCM_STREAM_ID;

	audio->out_buffer_size = BUFSZ;
	audio->out_sample_rate = 44100;
	audio->out_channel_mode = AUDPP_CMD_PCM_INTF_STEREO_V;
	audio->out_weight = 100;

	audio->out[0].data = audio->data + 0;
	audio->out[0].addr = audio->phys + 0;
	audio->out[0].size = BUFSZ;

	audio->out[1].data = audio->data + BUFSZ;
	audio->out[1].addr = audio->phys + BUFSZ;
	audio->out[1].size = BUFSZ;

	audio->vol_pan.volume = 0x2000;
	audio->vol_pan.pan = 0x0;
	audio->source = 0x0;

	audio_flush(audio);
	audio->voice_state = msm_get_voice_state();
	MM_DBG("voice_state = %x\n", audio->voice_state);
	audio->device_events = AUDDEV_EVT_DEV_RDY
				|AUDDEV_EVT_DEV_RLS|
				AUDDEV_EVT_STREAM_VOL_CHG|
				AUDDEV_EVT_VOICE_STATE_CHG;

	MM_DBG("register for event callback pdata %p\n", audio);
	rc = auddev_register_evt_listner(audio->device_events,
					AUDDEV_CLNT_DEC,
					audio->dec_id,
					audio_out_listener,
					(void *)audio);
	if (rc) {
		MM_ERR("%s: failed to register listener\n", __func__);
		goto done;
	}

	file->private_data = audio;
	audio->opened = 1;
	rc = 0;
done:
	mutex_unlock(&audio->lock);
	return rc;
}

static const struct file_operations audio_fops = {
	.owner		= THIS_MODULE,
	.open		= audio_open,
	.release	= audio_release,
	.read		= audio_read,
	.write		= audio_write,
	.unlocked_ioctl	= audio_ioctl,
	.fsync		= audio_fsync,
};

struct miscdevice audio_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_pcm_out",
	.fops	= &audio_fops,
};

static int __init audio_init(void)
{
	unsigned long ionflag = 0;
	ion_phys_addr_t addr = 0;
	int rc;
	int len = 0;
	struct ion_handle *handle = NULL;
	struct ion_client *client = NULL;

	client = msm_ion_client_create(UINT_MAX, "HostPCM");
	if (IS_ERR_OR_NULL(client)) {
		MM_ERR("Unable to create ION client\n");
		rc = -ENOMEM;
		goto client_create_error;
	}
	the_audio.client = client;

	handle = ion_alloc(client, DMASZ, SZ_4K,
		ION_HEAP(ION_AUDIO_HEAP_ID), 0);
	if (IS_ERR_OR_NULL(handle)) {
		MM_ERR("Unable to create allocate O/P buffers\n");
		rc = -ENOMEM;
		goto buff_alloc_error;
		}
	the_audio.buff_handle = handle;

	rc = ion_phys(client, handle, &addr, &len);
	if (rc) {
		MM_ERR("O/P buffers:Invalid phy: %x sz: %x\n",
			(unsigned int) addr, (unsigned int) len);
		goto buff_get_phys_error;
	} else
		MM_INFO("O/P buffers:valid phy: %x sz: %x\n",
			(unsigned int) addr, (unsigned int) len);
	the_audio.phys = (int32_t)addr;

	rc = ion_handle_get_flags(client, handle, &ionflag);
	if (rc) {
		MM_ERR("could not get flags for the handle\n");
		goto buff_get_flags_error;
	}

	the_audio.map_v_write = ion_map_kernel(client, handle);
	if (IS_ERR(the_audio.map_v_write)) {
		MM_ERR("could not map write buffers\n");
		rc = -ENOMEM;
		goto buff_map_error;
	}
	the_audio.data = (char *)the_audio.map_v_write;
	MM_DBG("Memory addr = 0x%8x  phy addr = 0x%8x\n",\
		(int) the_audio.data, (int) the_audio.phys);
	mutex_init(&the_audio.lock);
	mutex_init(&the_audio.write_lock);
	spin_lock_init(&the_audio.dsp_lock);
	init_waitqueue_head(&the_audio.wait);
	wake_lock_init(&the_audio.wakelock, WAKE_LOCK_SUSPEND, "audio_pcm");
	pm_qos_add_request(&the_audio.pm_qos_req, PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);
	return misc_register(&audio_misc);
buff_map_error:
buff_get_phys_error:
buff_get_flags_error:
	ion_free(client, the_audio.buff_handle);
buff_alloc_error:
	ion_client_destroy(client);
client_create_error:
	return rc;

}

late_initcall(audio_init);
