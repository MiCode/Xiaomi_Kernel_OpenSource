/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/msm_audio.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <mach/debug_mm.h>
#include <mach/msm_rpcrouter.h>

#define SND_VOC_PCM_INTERFACE_PROG	0x30000002
#define SND_VOC_PCM_INTERFACE_VERS	0x00020004

/* Supply always 160 words of PCM samples (20ms data) */
#define MAX_VOC_FRAME_SIZE 160
#define VOC_FRAME_DURATION 20
/* Buffering for Maximum 8 frames between userspace and driver */
#define MAX_VOC_FRAMES 8
#define BUFSZ ((MAX_VOC_FRAME_SIZE*2)*MAX_VOC_FRAMES)
#define SND_VOC_PCM_CLIENT_INPUT_FN_TYPE_PROC 3
#define SND_VOC_REGISTER_PCM_INPUT_CLIENT_PROC 24

#define START_CALLBACK_ID 0x12345678
#define STOP_CALLBACK_ID 0xffffffff

#define MAX_WAIT_CONSUME (MAX_VOC_FRAMES * VOC_FRAME_DURATION)
/* PCM Interfaces */
enum voice_pcm_interface_type {
	VOICE_PCM_INTERFACE_TX_INPUT = 3, /* PCM Inject input to PreProc */
};

enum voice_pcm_interface_reg_status_type {
	SUCCESS = 0, /* Success 0, else failure */
};

/* status used by PCM input callbacks to indicate availability of PCM Data */
enum voice_pcm_data_status_type {
	VOICE_PCM_DATA_STATUS_AVAILABLE,    /* Data available for PCM input */
	VOICE_PCM_DATA_STATUS_UNAVAILABLE,  /* Data not available           */
	VOICE_PCM_DATA_STATUS_MAX
};

/* Argument needed to register PCM input  client */
struct snd_voice_pcm_interface_ipclnt_reg_args {
	/* Interface number specifies the PCM inject point */
	enum voice_pcm_interface_type interface;
	/* Non-NULL indicates start,NULL indicates stop */
	uint32_t callback_id;
};

struct snd_voice_pcm_interface_ipclnt_reg_status {
	enum voice_pcm_interface_reg_status_type status;
};

struct snd_voice_pcm_interface_ipclnt_fn_type_args {
	uint32_t callback_id;
	uint32_t pcm_data_ptr_not_null;
	uint32_t pcm_data_max_length;
};

struct snd_voice_pcm_interface_ipclnt_fn_type_reply {
	enum voice_pcm_data_status_type status;
	struct {
		uint32_t pcm_data_len;
		struct {
			uint16_t pcm_data_ignore;
			uint16_t pcm_data_valid;
		} pcm_data_val[MAX_VOC_FRAME_SIZE];
	} pcm_data;
};

struct buffer {
	void *data;
	unsigned size;
	unsigned used;
};

struct audio {
	struct buffer out[MAX_VOC_FRAMES];

	uint8_t out_head;
	uint8_t out_tail;

	atomic_t out_bytes;
	/* data allocated for various buffers */
	char *data;

	struct mutex lock;
	struct mutex write_lock;
	wait_queue_head_t wait;
	wait_queue_head_t stop_wait;

	int buffer_finished;
	int opened;
	int enabled;
	int running;
	int stopped; /* set when stopped */

	struct msm_rpc_client *client;
};

static struct audio the_audio;

static int snd_voice_pcm_interface_ipclnt_reg_args(
	struct msm_rpc_client *client, void *buf, void *data)
{
	struct snd_voice_pcm_interface_ipclnt_reg_args *arg;
	int size = 0;

	arg = (struct snd_voice_pcm_interface_ipclnt_reg_args *)data;
	*((int *)buf) = cpu_to_be32(arg->interface);
	size += sizeof(int);
	buf += sizeof(int);
	*((int *)buf) = cpu_to_be32(arg->callback_id);
	size += sizeof(int);

	return size;
}

static int snd_voice_pcm_interface_ipclnt_reg_status(
	struct msm_rpc_client *client, void *buf, void *data)
{
	struct snd_voice_pcm_interface_ipclnt_reg_status *result =
	(struct snd_voice_pcm_interface_ipclnt_reg_status *)buf;

	*((int *)data) =  be32_to_cpu(result->status);
	return 0;
}

static void process_callback(struct audio *audio,
	void *buffer, int in_size)
{
	uint32_t accept_status = RPC_ACCEPTSTAT_SUCCESS;
	struct rpc_request_hdr *req;
	struct snd_voice_pcm_interface_ipclnt_fn_type_args arg, *buf_ptr;
	struct snd_voice_pcm_interface_ipclnt_fn_type_reply *reply;
	struct buffer *frame;
	uint32_t status;
	uint32_t pcm_data_len;

	req = (struct rpc_request_hdr *)buffer;
	buf_ptr = (struct snd_voice_pcm_interface_ipclnt_fn_type_args *)\
				(req + 1);
	arg.callback_id = be32_to_cpu(buf_ptr->callback_id);
	arg.pcm_data_ptr_not_null = be32_to_cpu(buf_ptr->pcm_data_ptr_not_null);
	arg.pcm_data_max_length = be32_to_cpu(buf_ptr->pcm_data_max_length);

	MM_DBG("callback_id = 0x%8x pcm_data_ptr_not_null = 0x%8x"\
		"pcm_data_max_length = 0x%8x\n", arg.callback_id,\
		arg.pcm_data_ptr_not_null, arg.pcm_data_max_length);
	/* Flag interface as running */
	if (!audio->running)
		audio->running = 1;
	if (!arg.pcm_data_ptr_not_null) {
		accept_status = RPC_ACCEPTSTAT_SYSTEM_ERR;
		msm_rpc_start_accepted_reply(audio->client,
			be32_to_cpu(req->xid), accept_status);
		msm_rpc_send_accepted_reply(audio->client, 0);
		return;
	}
	reply = (struct snd_voice_pcm_interface_ipclnt_fn_type_reply *)
		msm_rpc_start_accepted_reply(audio->client,
			be32_to_cpu(req->xid), accept_status);
	frame = audio->out + audio->out_tail;
	/* If Data available, send data */
	if (frame->used) {
		int i;
		unsigned short *src = frame->data;
		atomic_add(frame->used, &audio->out_bytes);
		status = VOICE_PCM_DATA_STATUS_AVAILABLE;
		pcm_data_len = MAX_VOC_FRAME_SIZE;
		xdr_send_int32(&audio->client->cb_xdr, &status);
		xdr_send_int32(&audio->client->cb_xdr, &pcm_data_len);
		/* Expected cb_xdr buffer size is more than PCM buffer size */
		for (i = 0; i < MAX_VOC_FRAME_SIZE; i++, ++src)
			xdr_send_int16(&audio->client->cb_xdr, src);
		frame->used = 0;
		audio->out_tail = ((++audio->out_tail) % MAX_VOC_FRAMES);
		wake_up(&audio->wait);
	} else {
		status = VOICE_PCM_DATA_STATUS_UNAVAILABLE;
		pcm_data_len = 0;
		xdr_send_int32(&audio->client->cb_xdr, &status);
		xdr_send_int32(&audio->client->cb_xdr, &pcm_data_len);
		wake_up(&audio->wait);
		/* Flag all buffer completed */
		if (audio->stopped) {
			audio->buffer_finished = 1;
			wake_up(&audio->stop_wait);
		}
	}
	MM_DBG("Provided PCM data = 0x%8x\n", reply->status);
	msm_rpc_send_accepted_reply(audio->client, 0);
	return;
}

static int pcm_interface_process_callback_routine(struct msm_rpc_client *client,
	void *buffer, int in_size)
{
	struct rpc_request_hdr *req;
	struct audio *audio = &the_audio;
	int rc = 0;

	req = (struct rpc_request_hdr *)buffer;

	MM_DBG("proc id = 0x%8x xid = 0x%8x size = 0x%8x\n",
		be32_to_cpu(req->procedure), be32_to_cpu(req->xid), in_size);
	switch (be32_to_cpu(req->procedure)) {
	/* Procedure which called every 20ms for PCM samples request*/
	case SND_VOC_PCM_CLIENT_INPUT_FN_TYPE_PROC:
		process_callback(audio, buffer, in_size);
		break;
	default:
		MM_ERR("Not supported proceudure 0x%8x\n",
			be32_to_cpu(req->procedure));
		/* Not supported RPC Procedure, send nagative code */
		msm_rpc_start_accepted_reply(client, be32_to_cpu(req->xid),
				RPC_ACCEPTSTAT_PROC_UNAVAIL);
		msm_rpc_send_accepted_reply(client, 0);
	}
	return rc;
}

static void audio_flush(struct audio *audio)
{
	int cnt;
	for (cnt = 0; cnt < MAX_VOC_FRAMES; cnt++)
		audio->out[cnt].used = 0;
	audio->out_head = 0;
	audio->out_tail = 0;
	audio->stopped = 0;
	audio->running = 0;
	audio->buffer_finished = 0;
}

/* must be called with audio->lock held */
static int audio_enable(struct audio *audio)
{
	int rc;
	struct snd_voice_pcm_interface_ipclnt_reg_args arg;
	struct snd_voice_pcm_interface_ipclnt_reg_status result;

	/* voice_pcm_interface_type */
	arg.interface = VOICE_PCM_INTERFACE_TX_INPUT;
	/* Should be non-zero, unique */
	arg.callback_id = START_CALLBACK_ID;
	/* Start Voice PCM interface */
	rc = msm_rpc_client_req(audio->client,
				SND_VOC_REGISTER_PCM_INPUT_CLIENT_PROC,
				snd_voice_pcm_interface_ipclnt_reg_args, &arg,
				snd_voice_pcm_interface_ipclnt_reg_status,
				&result, -1);
	MM_DBG("input client registration status rc 0x%8x result 0x%8x\n",
		rc, result.status);
	/* If error in server side */
	if (rc == 0)
		if (result.status != SUCCESS)
			rc = -ENODEV;
	return rc;
}
static int audio_disable(struct audio *audio)
{
	int rc;
	struct snd_voice_pcm_interface_ipclnt_reg_args arg;
	struct snd_voice_pcm_interface_ipclnt_reg_status result;

	/* Wait till all buffers consumed to prevent data loss
	   Also ensure if client stops due to vocoder disable
	   do not loop forever */
	rc = wait_event_interruptible_timeout(audio->stop_wait,
		!(audio->running) || (audio->buffer_finished == 1),
		msecs_to_jiffies(MAX_WAIT_CONSUME));
	if (rc < 0)
		return 0;
	/* voice_pcm_interface_type */
	arg.interface = VOICE_PCM_INTERFACE_TX_INPUT;
	arg.callback_id = STOP_CALLBACK_ID; /* Should be zero */
	/* Stop Voice PCM interface */
	rc = msm_rpc_client_req(audio->client,
				SND_VOC_REGISTER_PCM_INPUT_CLIENT_PROC,
				snd_voice_pcm_interface_ipclnt_reg_args, &arg,
				snd_voice_pcm_interface_ipclnt_reg_status,
				&result, -1);
	MM_DBG("input client de-registration status rc 0x%8x result 0x%8x\n",
		rc, result.status);
	/* If error in server side */
	if (rc == 0)
		if (result.status != SUCCESS)
			rc = -ENODEV;
	return rc;
}

static long audio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct audio *audio = file->private_data;
	int rc = -EINVAL;

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;
		stats.byte_count = atomic_read(&audio->out_bytes);
		if (copy_to_user((void *) arg, &stats, sizeof(stats)))
			return -EFAULT;
		return 0;
	}

	mutex_lock(&audio->lock);
	switch (cmd) {
	case AUDIO_START:
		rc = audio_enable(audio);
		if (rc == 0)
			audio->enabled = 1;
		break;
	case AUDIO_STOP:
		if (audio->enabled) {
			audio->stopped = 1;
			rc = audio_disable(audio);
			if (rc == 0) {
				audio->enabled = 0;
				audio->running = 0;
				wake_up(&audio->wait);
			} else
				audio->stopped = 0;
		}
		break;
	case AUDIO_SET_CONFIG: {
		struct msm_audio_config config;
		if (copy_from_user(&config, (void *) arg, sizeof(config))) {
			rc = -EFAULT;
			break;
		}
		if (config.type == 0) {
			/* Selection for different PCM intect point */
		} else {
			rc = -EINVAL;
			break;
		}
		rc = 0;
		break;
	}
	case AUDIO_GET_CONFIG: {
		struct msm_audio_config config;
		config.buffer_size = MAX_VOC_FRAME_SIZE * 2;
		config.buffer_count = MAX_VOC_FRAMES;
		config.sample_rate = 8000;
		config.channel_count = 1;
		config.type = 0;
		config.unused[0] = 0;
		config.unused[1] = 0;
		config.unused[2] = 0;
		if (copy_to_user((void *) arg, &config, sizeof(config)))
			rc = -EFAULT;
		else
			rc = 0;
		break;
	}
	default: {
		rc = -EINVAL;
		MM_ERR(" Unsupported ioctl 0x%8x\n", cmd);
	}
	}
	mutex_unlock(&audio->lock);
	return rc;
}
static ssize_t audio_read(struct file *file, char __user *buf,
		size_t count, loff_t *pos)
{
	return -EINVAL;
}

static ssize_t audio_write(struct file *file, const char __user *buf,
		size_t count, loff_t *pos)
{
	struct audio *audio = file->private_data;
	const char __user *start = buf;
	struct buffer *frame;
	size_t xfer;
	int rc = 0;

	mutex_lock(&audio->write_lock);
	/* Ensure to copy only till frame boundary */
	while (count >= (MAX_VOC_FRAME_SIZE*2)) {
		frame = audio->out + audio->out_head;
		rc = wait_event_interruptible_timeout(audio->wait,\
				(frame->used == 0) || (audio->stopped),
				msecs_to_jiffies(MAX_WAIT_CONSUME));

		if (rc < 0)
			break;
		if (audio->stopped) {
			rc = -EBUSY;
			break;
		}
		if (rc == 0) {
			rc = -ETIMEDOUT;
			break;
		}

		xfer = count > frame->size ? frame->size : count;
		if (copy_from_user(frame->data, buf, xfer)) {
			rc = -EFAULT;
			break;
		}
		frame->used = xfer;
		audio->out_head = ((++audio->out_head) % MAX_VOC_FRAMES);
		count -= xfer;
		buf += xfer;
	}
	mutex_unlock(&audio->write_lock);
	MM_DBG("write done 0x%8x\n", (unsigned int)(buf - start));
	if (rc < 0)
		return rc;
	return buf - start;
}

static int audio_open(struct inode *inode, struct file *file)
{
	struct audio *audio = &the_audio;
	int rc, cnt;

	mutex_lock(&audio->lock);

	if (audio->opened) {
		MM_ERR("busy as driver already in open state\n");
		rc = -EBUSY;
		goto done;
	}

	if (!audio->data) {
		audio->data = kmalloc(BUFSZ, GFP_KERNEL);
		if (!audio->data) {
			MM_ERR("could not allocate buffers\n");
			rc = -ENOMEM;
			goto done;
		}
	}

	audio->client = msm_rpc_register_client("voice_pcm_interface_client",
				SND_VOC_PCM_INTERFACE_PROG,
				SND_VOC_PCM_INTERFACE_VERS, 1,
				pcm_interface_process_callback_routine);
	if (IS_ERR(audio->client)) {
		MM_ERR("Failed to register voice pcm interface client"\
			"to 0x%8x\n", SND_VOC_PCM_INTERFACE_PROG);
		kfree(audio->data);
		audio->data = NULL;
		rc = -ENODEV;
		goto done;
	}
	MM_INFO("voice pcm client registred %p\n", audio->client);
	for (cnt = 0; cnt < MAX_VOC_FRAMES; cnt++) {
		audio->out[cnt].data = (audio->data +\
					((MAX_VOC_FRAME_SIZE * 2) * cnt));
		audio->out[cnt].size = MAX_VOC_FRAME_SIZE * 2;
		MM_DBG("data ptr = %p\n", audio->out[cnt].data);
	}
	file->private_data = audio;
	audio_flush(audio);
	audio->opened = 1;
	rc = 0;
done:
	mutex_unlock(&audio->lock);
	return rc;
}

static int audio_release(struct inode *inode, struct file *file)
{
	struct audio *audio = file->private_data;

	mutex_lock(&audio->lock);
	if (audio->enabled) {
		audio->stopped = 1;
		audio_disable(audio);
		audio->running = 0;
		audio->enabled = 0;
		wake_up(&audio->wait);
	}
	msm_rpc_unregister_client(audio->client);
	kfree(audio->data);
	audio->data = NULL;
	audio->opened = 0;
	mutex_unlock(&audio->lock);
	return 0;
}

static const struct file_operations audio_fops = {
	.owner		= THIS_MODULE,
	.open		= audio_open,
	.release	= audio_release,
	.read		= audio_read,
	.write		= audio_write,
	.unlocked_ioctl	= audio_ioctl,
};

static struct miscdevice audio_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "snd_pcm_client",
	.fops	= &audio_fops,
};

static int __init audio_init(void)
{
	mutex_init(&the_audio.lock);
	mutex_init(&the_audio.write_lock);
	init_waitqueue_head(&the_audio.wait);
	init_waitqueue_head(&the_audio.stop_wait);
	return misc_register(&audio_misc);
}
device_initcall(audio_init);
