/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/msm_audio.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <mach/qdsp5v2/audio_dev_ctl.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <mach/debug_mm.h>
#include <mach/qdsp5v2/qdsp5audppmsg.h>
#include <mach/qdsp5v2/audpp.h>
#include <linux/slab.h>
#include <linux/debugfs.h>

#ifndef MAX
#define  MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif


static DEFINE_MUTEX(session_lock);

struct audio_dev_ctrl_state {
	struct msm_snddev_info *devs[AUDIO_DEV_CTL_MAX_DEV];
	u32 num_dev;
	atomic_t opened;
	struct msm_snddev_info *voice_rx_dev;
	struct msm_snddev_info *voice_tx_dev;
	wait_queue_head_t      wait;
};

static struct audio_dev_ctrl_state audio_dev_ctrl;
struct event_listner event;
#define MAX_DEC_SESSIONS	7
#define MAX_ENC_SESSIONS	3

struct session_freq {
	int freq;
	int evt;
};


struct audio_routing_info {
	unsigned short mixer_mask[MAX_DEC_SESSIONS];
	unsigned short audrec_mixer_mask[MAX_ENC_SESSIONS];
	struct session_freq dec_freq[MAX_DEC_SESSIONS];
	struct session_freq enc_freq[MAX_ENC_SESSIONS];
	int dual_mic_setting[MAX_ENC_SESSIONS];
	int voice_tx_dev_id;
	int voice_rx_dev_id;
	int voice_tx_sample_rate;
	int voice_rx_sample_rate;
	signed int voice_tx_vol;
	signed int voice_rx_vol;
	int tx_mute;
	int rx_mute;
	int voice_state;
};

static struct audio_routing_info routing_info;

#ifdef CONFIG_DEBUG_FS

static struct dentry *dentry;
static int rtc_getdevice_dbg_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	MM_INFO("debug intf %s\n", (char *) file->private_data);
	return 0;
}
bool is_dev_opened(u32 adb_id)
{

	int dev_id = 0;
	struct msm_snddev_info *dev_info = NULL;

	for (dev_id = 0; dev_id < audio_dev_ctrl.num_dev; dev_id++) {
		dev_info = audio_dev_ctrl_find_dev(dev_id);
	      if (IS_ERR(dev_info)) {
		MM_ERR("pass invalid dev_id %d\n", dev_id);
			  return false;
		}
		if (dev_info->opened && (dev_info->acdb_id == adb_id))
			return true;
	}

  return false;
}
static ssize_t rtc_getdevice_dbg_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	static char buffer[1024];
	static char swap_buf[1024];
	const int debug_bufmax = sizeof(buffer);
	int n = 0;
	int swap_count = 0;
	int rc = 0;
    int dev_count = 0;
	int dev_id = 0;
	struct msm_snddev_info *dev_info = NULL;


	if (audio_dev_ctrl.num_dev <= 0) {
		MM_ERR("Invalid no Device present\n");
		dev_count = 0;
		n = scnprintf(buffer, debug_bufmax, "DEV_NO:0x%x\n", dev_count);
	} else {
	for (dev_id = 0; dev_id < audio_dev_ctrl.num_dev; dev_id++) {
		dev_info = audio_dev_ctrl_find_dev(dev_id);
		if (IS_ERR(dev_info)) {
			MM_ERR("pass invalid dev_id %d\n", dev_id);
			rc = PTR_ERR(dev_info);
			return rc;
		}
		if (dev_info->opened) {
			n += scnprintf(swap_buf + n, debug_bufmax - n,
					"ACDB_ID:0x%x;CAPB:0x%x\n",
					dev_info->acdb_id,
					dev_info->capability);
		      dev_count++;
		      MM_DBG("RTC Get Device %x COPP %x Session Mask \
			      %x Capb %x Dev Count %x\n",
			     dev_id , dev_info->copp_id, dev_info->sessions,
			     dev_info->capability, dev_count);

		}
	}

	swap_count = scnprintf(buffer, debug_bufmax, \
			"DEV_NO:0x%x\n", dev_count);

	memcpy(buffer+swap_count, swap_buf, n*sizeof(char));
	n = n+swap_count;

	buffer[n] = 0;
    }
	return simple_read_from_buffer(buf, count, ppos, buffer, n);
}

static const struct file_operations rtc_acdb_debug_fops = {
	.open = rtc_getdevice_dbg_open,
	.read = rtc_getdevice_dbg_read
};
#endif
int msm_reset_all_device(void)
{
	int rc = 0;
	int dev_id = 0;
	struct msm_snddev_info *dev_info = NULL;

	for (dev_id = 0; dev_id < audio_dev_ctrl.num_dev; dev_id++) {
		dev_info = audio_dev_ctrl_find_dev(dev_id);
		if (IS_ERR(dev_info)) {
			MM_ERR("pass invalid dev_id %d\n", dev_id);
			rc = PTR_ERR(dev_info);
			return rc;
		}
		if (!dev_info->opened)
			continue;
		MM_DBG("Resetting device %d active on COPP %d"
			"with  0x%08x as routing\n",
				dev_id, dev_info->copp_id, dev_info->sessions);
		broadcast_event(AUDDEV_EVT_REL_PENDING,
					dev_id,
					SESSION_IGNORE);
		rc = dev_info->dev_ops.close(dev_info);
		if (rc < 0) {
			MM_ERR("Snd device %d failed close!\n", dev_id);
			return rc;
		} else {
			dev_info->opened = 0;
			broadcast_event(AUDDEV_EVT_DEV_RLS,
				dev_id,
				SESSION_IGNORE);
		}
		dev_info->sessions = 0;
	}
	return 0;
}
EXPORT_SYMBOL(msm_reset_all_device);

int msm_set_dual_mic_config(int enc_session_id, int config)
{
	int i;
	if (enc_session_id >= MAX_ENC_SESSIONS)
		return -EINVAL;
	/*config is set(1) dual mic recording is selected */
	/*config is reset (0) dual mic recording is not selected*/
	routing_info.dual_mic_setting[enc_session_id] = config;
	for (i = 0; i < MAX_ENC_SESSIONS; i++)
		MM_DBG("dual_mic_setting[%d] = %d\n",
			i, routing_info.dual_mic_setting[i]);
	return 0;
}
EXPORT_SYMBOL(msm_set_dual_mic_config);

int msm_get_dual_mic_config(int enc_session_id)
{
	if (enc_session_id >= MAX_ENC_SESSIONS)
		return -EINVAL;
	return routing_info.dual_mic_setting[enc_session_id];
}
EXPORT_SYMBOL(msm_get_dual_mic_config);

int msm_get_voice_state(void)
{
	MM_DBG("voice state %d\n", routing_info.voice_state);
	return routing_info.voice_state;
}
EXPORT_SYMBOL(msm_get_voice_state);

int msm_set_voice_mute(int dir, int mute)
{
	MM_DBG("dir %x mute %x\n", dir, mute);
	if (!audio_dev_ctrl.voice_rx_dev
		|| !audio_dev_ctrl.voice_tx_dev)
		return -EPERM;
	if (dir == DIR_TX) {
		routing_info.tx_mute = mute;
		broadcast_event(AUDDEV_EVT_DEVICE_VOL_MUTE_CHG,
			routing_info.voice_tx_dev_id, SESSION_IGNORE);
	} else
		return -EPERM;
	return 0;
}
EXPORT_SYMBOL(msm_set_voice_mute);

int msm_set_voice_vol(int dir, s32 volume)
{
	if (!audio_dev_ctrl.voice_rx_dev
		|| !audio_dev_ctrl.voice_tx_dev)
		return -EPERM;
	if (dir == DIR_TX) {
		routing_info.voice_tx_vol = volume;
		broadcast_event(AUDDEV_EVT_DEVICE_VOL_MUTE_CHG,
					routing_info.voice_tx_dev_id,
					SESSION_IGNORE);
	} else if (dir == DIR_RX) {
		routing_info.voice_rx_vol = volume;
		broadcast_event(AUDDEV_EVT_DEVICE_VOL_MUTE_CHG,
					routing_info.voice_rx_dev_id,
					SESSION_IGNORE);
	} else
		return -EINVAL;
	return 0;
}
EXPORT_SYMBOL(msm_set_voice_vol);

void msm_snddev_register(struct msm_snddev_info *dev_info)
{
	mutex_lock(&session_lock);
	if (audio_dev_ctrl.num_dev < AUDIO_DEV_CTL_MAX_DEV) {
		audio_dev_ctrl.devs[audio_dev_ctrl.num_dev] = dev_info;
		dev_info->dev_volume = 50; /* 50%  */
		dev_info->sessions = 0x0;
		dev_info->usage_count = 0;
		dev_info->set_sample_rate = 0;
		audio_dev_ctrl.num_dev++;
	} else
		MM_ERR("%s: device registry max out\n", __func__);
	mutex_unlock(&session_lock);
}
EXPORT_SYMBOL(msm_snddev_register);

int msm_snddev_devcount(void)
{
	return audio_dev_ctrl.num_dev;
}
EXPORT_SYMBOL(msm_snddev_devcount);

int msm_snddev_query(int dev_id)
{
	if (dev_id <= audio_dev_ctrl.num_dev)
			return 0;
	return -ENODEV;
}
EXPORT_SYMBOL(msm_snddev_query);

int msm_snddev_is_set(int popp_id, int copp_id)
{
	return routing_info.mixer_mask[popp_id] & (0x1 << copp_id);
}
EXPORT_SYMBOL(msm_snddev_is_set);

unsigned short msm_snddev_route_enc(int enc_id)
{
	if (enc_id >= MAX_ENC_SESSIONS)
		return -EINVAL;
	return routing_info.audrec_mixer_mask[enc_id];
}
EXPORT_SYMBOL(msm_snddev_route_enc);

unsigned short msm_snddev_route_dec(int popp_id)
{
	if (popp_id >= MAX_DEC_SESSIONS)
		return -EINVAL;
	return routing_info.mixer_mask[popp_id];
}
EXPORT_SYMBOL(msm_snddev_route_dec);

int msm_snddev_set_dec(int popp_id, int copp_id, int set)
{
	if (set)
		routing_info.mixer_mask[popp_id] |= (0x1 << copp_id);
	else
		routing_info.mixer_mask[popp_id] &= ~(0x1 << copp_id);

	return 0;
}
EXPORT_SYMBOL(msm_snddev_set_dec);

int msm_snddev_set_enc(int popp_id, int copp_id, int set)
{
	if (set)
		routing_info.audrec_mixer_mask[popp_id] |= (0x1 << copp_id);
	else
		routing_info.audrec_mixer_mask[popp_id] &= ~(0x1 << copp_id);
	return 0;
}
EXPORT_SYMBOL(msm_snddev_set_enc);

int msm_device_is_voice(int dev_id)
{
	if ((dev_id == routing_info.voice_rx_dev_id)
		|| (dev_id == routing_info.voice_tx_dev_id))
		return 0;
	else
		return -EINVAL;
}
EXPORT_SYMBOL(msm_device_is_voice);

int msm_set_voc_route(struct msm_snddev_info *dev_info,
			int stream_type, int dev_id)
{
	int rc = 0;
	u32 session_mask = 0;

	mutex_lock(&session_lock);
	switch (stream_type) {
	case AUDIO_ROUTE_STREAM_VOICE_RX:
		if (audio_dev_ctrl.voice_rx_dev)
			audio_dev_ctrl.voice_rx_dev->sessions &= ~0xFF;

		if (!(dev_info->capability & SNDDEV_CAP_RX) |
		    !(dev_info->capability & SNDDEV_CAP_VOICE)) {
			rc = -EINVAL;
			break;
		}
		audio_dev_ctrl.voice_rx_dev = dev_info;
		if (audio_dev_ctrl.voice_rx_dev) {
			session_mask =
				0x1 << (8 * ((int)AUDDEV_CLNT_VOC-1));
			audio_dev_ctrl.voice_rx_dev->sessions |=
				session_mask;
		}
		routing_info.voice_rx_dev_id = dev_id;
		break;
	case AUDIO_ROUTE_STREAM_VOICE_TX:
		if (audio_dev_ctrl.voice_tx_dev)
			audio_dev_ctrl.voice_tx_dev->sessions &= ~0xFF;

		if (!(dev_info->capability & SNDDEV_CAP_TX) |
		    !(dev_info->capability & SNDDEV_CAP_VOICE)) {
			rc = -EINVAL;
			break;
		}

		audio_dev_ctrl.voice_tx_dev = dev_info;
		if (audio_dev_ctrl.voice_rx_dev) {
			session_mask =
				0x1 << (8 * ((int)AUDDEV_CLNT_VOC-1));
			audio_dev_ctrl.voice_tx_dev->sessions |=
				session_mask;
		}
		routing_info.voice_tx_dev_id = dev_id;
		break;
	default:
		rc = -EINVAL;
	}
	mutex_unlock(&session_lock);
	return rc;
}
EXPORT_SYMBOL(msm_set_voc_route);

void msm_release_voc_thread(void)
{
	wake_up(&audio_dev_ctrl.wait);
}
EXPORT_SYMBOL(msm_release_voc_thread);

int msm_snddev_get_enc_freq(session_id)
{
	return routing_info.enc_freq[session_id].freq;
}
EXPORT_SYMBOL(msm_snddev_get_enc_freq);

int msm_get_voc_freq(int *tx_freq, int *rx_freq)
{
	*tx_freq = routing_info.voice_tx_sample_rate;
	*rx_freq = routing_info.voice_rx_sample_rate;
	return 0;
}
EXPORT_SYMBOL(msm_get_voc_freq);

int msm_get_voc_route(u32 *rx_id, u32 *tx_id)
{
	int rc = 0;

	if (!rx_id || !tx_id)
		return -EINVAL;

	mutex_lock(&session_lock);
	if (!audio_dev_ctrl.voice_rx_dev || !audio_dev_ctrl.voice_tx_dev) {
		rc = -ENODEV;
		mutex_unlock(&session_lock);
		return rc;
	}

	*rx_id = audio_dev_ctrl.voice_rx_dev->acdb_id;
	*tx_id = audio_dev_ctrl.voice_tx_dev->acdb_id;

	mutex_unlock(&session_lock);

	return rc;
}
EXPORT_SYMBOL(msm_get_voc_route);

struct msm_snddev_info *audio_dev_ctrl_find_dev(u32 dev_id)
{
	struct msm_snddev_info *info;

	if ((audio_dev_ctrl.num_dev - 1) < dev_id) {
		info = ERR_PTR(-ENODEV);
		goto error;
	}

	info = audio_dev_ctrl.devs[dev_id];
error:
	return info;

}
EXPORT_SYMBOL(audio_dev_ctrl_find_dev);

int snddev_voice_set_volume(int vol, int path)
{
	if (audio_dev_ctrl.voice_rx_dev
		&& audio_dev_ctrl.voice_tx_dev) {
		if (path)
			audio_dev_ctrl.voice_tx_dev->dev_volume = vol;
		else
			audio_dev_ctrl.voice_rx_dev->dev_volume = vol;
	} else
		return -ENODEV;
	return 0;
}
EXPORT_SYMBOL(snddev_voice_set_volume);

static int audio_dev_ctrl_get_devices(struct audio_dev_ctrl_state *dev_ctrl,
				      void __user *arg)
{
	int rc = 0;
	u32 index;
	struct msm_snd_device_list work_list;
	struct msm_snd_device_info *work_tbl;

	if (copy_from_user(&work_list, arg, sizeof(work_list))) {
		rc = -EFAULT;
		goto error;
	}

	if (work_list.num_dev > dev_ctrl->num_dev) {
		rc = -EINVAL;
		goto error;
	}

	work_tbl = kmalloc(work_list.num_dev *
		sizeof(struct msm_snd_device_info), GFP_KERNEL);
	if (!work_tbl) {
		rc = -ENOMEM;
		goto error;
	}

	for (index = 0; index < dev_ctrl->num_dev; index++) {
		work_tbl[index].dev_id = index;
		work_tbl[index].dev_cap = dev_ctrl->devs[index]->capability;
		strlcpy(work_tbl[index].dev_name, dev_ctrl->devs[index]->name,
		64);
	}

	if (copy_to_user((void *) (work_list.list), work_tbl,
		 work_list.num_dev * sizeof(struct msm_snd_device_info)))
		rc = -EFAULT;
	kfree(work_tbl);
error:
	return rc;
}


int auddev_register_evt_listner(u32 evt_id, u32 clnt_type, u32 clnt_id,
		void (*listner)(u32 evt_id,
			union auddev_evt_data *evt_payload,
			void *private_data),
		void *private_data)
{
	int rc;
	struct msm_snd_evt_listner *callback = NULL;
	struct msm_snd_evt_listner *new_cb;

	new_cb = kzalloc(sizeof(struct msm_snd_evt_listner), GFP_KERNEL);
	if (!new_cb) {
		MM_ERR("No memory to add new listener node\n");
		return -ENOMEM;
	}

	mutex_lock(&session_lock);
	new_cb->cb_next = NULL;
	new_cb->auddev_evt_listener = listner;
	new_cb->evt_id = evt_id;
	new_cb->clnt_type = clnt_type;
	new_cb->clnt_id = clnt_id;
	new_cb->private_data = private_data;
	if (event.cb == NULL) {
		event.cb = new_cb;
		new_cb->cb_prev = NULL;
	} else {
		callback = event.cb;
		for (; ;) {
			if (callback->cb_next == NULL)
				break;
			else {
				callback = callback->cb_next;
				continue;
			}
		}
		callback->cb_next = new_cb;
		new_cb->cb_prev = callback;
	}
	event.num_listner++;
	mutex_unlock(&session_lock);
	rc = 0;
	return rc;
}
EXPORT_SYMBOL(auddev_register_evt_listner);

int auddev_unregister_evt_listner(u32 clnt_type, u32 clnt_id)
{
	struct msm_snd_evt_listner *callback = event.cb;
	struct msm_snddev_info *info;
	u32 session_mask = 0;
	int i = 0;

	mutex_lock(&session_lock);
	while (callback != NULL) {
		if ((callback->clnt_type == clnt_type)
			&& (callback->clnt_id == clnt_id))
			break;
		 callback = callback->cb_next;
	}
	if (callback == NULL) {
		mutex_unlock(&session_lock);
		return -EINVAL;
	}

	if ((callback->cb_next == NULL) && (callback->cb_prev == NULL))
		event.cb = NULL;
	else if (callback->cb_next == NULL)
		callback->cb_prev->cb_next = NULL;
	else if (callback->cb_prev == NULL) {
		callback->cb_next->cb_prev = NULL;
		event.cb = callback->cb_next;
	} else {
		callback->cb_prev->cb_next = callback->cb_next;
		callback->cb_next->cb_prev = callback->cb_prev;
	}
	kfree(callback);

	session_mask = (0x1 << (clnt_id)) << (8 * ((int)clnt_type-1));
	for (i = 0; i < audio_dev_ctrl.num_dev; i++) {
		info = audio_dev_ctrl.devs[i];
		info->sessions &= ~session_mask;
	}
	if (clnt_type == AUDDEV_CLNT_ENC)
		msm_set_dual_mic_config(clnt_id, 0);
	mutex_unlock(&session_lock);
	return 0;
}
EXPORT_SYMBOL(auddev_unregister_evt_listner);

int msm_snddev_withdraw_freq(u32 session_id, u32 capability, u32 clnt_type)
{
	int i = 0;
	struct msm_snddev_info *info;
	u32 session_mask = 0;

	if ((clnt_type == AUDDEV_CLNT_VOC) && (session_id != 0))
		return -EINVAL;
	if ((clnt_type == AUDDEV_CLNT_DEC)
			&& (session_id >= MAX_DEC_SESSIONS))
		return -EINVAL;
	if ((clnt_type == AUDDEV_CLNT_ENC)
			&& (session_id >= MAX_ENC_SESSIONS))
		return -EINVAL;

	session_mask = (0x1 << (session_id)) << (8 * ((int)clnt_type-1));

	for (i = 0; i < audio_dev_ctrl.num_dev; i++) {
		info = audio_dev_ctrl.devs[i];
		if ((info->sessions & session_mask)
			&& (info->capability & capability)) {
			if (!(info->sessions & ~(session_mask)))
				info->set_sample_rate = 0;
		}
	}
	if (clnt_type == AUDDEV_CLNT_DEC)
		routing_info.dec_freq[session_id].freq
					= 0;
	else if (clnt_type == AUDDEV_CLNT_ENC)
		routing_info.enc_freq[session_id].freq
					= 0;
	else if (capability == SNDDEV_CAP_TX)
		routing_info.voice_tx_sample_rate = 0;
	else
		routing_info.voice_rx_sample_rate = 48000;
	return 0;
}

int msm_snddev_request_freq(int *freq, u32 session_id,
			u32 capability, u32 clnt_type)
{
	int i = 0;
	int rc = 0;
	struct msm_snddev_info *info;
	u32 set_freq;
	u32 session_mask = 0;
	u32 clnt_type_mask = 0;

	MM_DBG(": clnt_type 0x%08x\n", clnt_type);

	if ((clnt_type == AUDDEV_CLNT_VOC) && (session_id != 0))
		return -EINVAL;
	if ((clnt_type == AUDDEV_CLNT_DEC)
			&& (session_id >= MAX_DEC_SESSIONS))
		return -EINVAL;
	if ((clnt_type == AUDDEV_CLNT_ENC)
			&& (session_id >= MAX_ENC_SESSIONS))
		return -EINVAL;
	session_mask = ((0x1 << session_id)) << (8 * (clnt_type-1));
	clnt_type_mask = (0xFF << (8 * (clnt_type-1)));
	if (!(*freq == 8000) && !(*freq == 11025) &&
		!(*freq == 12000) && !(*freq == 16000) &&
		!(*freq == 22050) && !(*freq == 24000) &&
		!(*freq == 32000) && !(*freq == 44100) &&
		!(*freq == 48000))
		return -EINVAL;

	for (i = 0; i < audio_dev_ctrl.num_dev; i++) {
		info = audio_dev_ctrl.devs[i];
		if ((info->sessions & session_mask)
			&& (info->capability & capability)) {
			rc = 0;
			if ((info->sessions & ~clnt_type_mask)
				&& ((*freq != 8000) && (*freq != 16000)
					&& (*freq != 48000))) {
				if (clnt_type == AUDDEV_CLNT_ENC) {
					routing_info.enc_freq[session_id].freq
							= 0;
					return -EPERM;
				} else if (clnt_type == AUDDEV_CLNT_DEC) {
					routing_info.dec_freq[session_id].freq
							= 0;
					return -EPERM;
				}
			}
			if (*freq == info->set_sample_rate) {
				rc = info->set_sample_rate;
				continue;
			}
			set_freq = MAX(*freq, info->set_sample_rate);


			if (clnt_type == AUDDEV_CLNT_DEC)
				routing_info.dec_freq[session_id].freq
						= set_freq;
			else if (clnt_type == AUDDEV_CLNT_ENC)
				routing_info.enc_freq[session_id].freq
						= set_freq;
			else if (capability == SNDDEV_CAP_TX)
				routing_info.voice_tx_sample_rate = set_freq;

			rc = set_freq;
			*freq = set_freq;
			/* There is difference in device sample rate to
			 * requested sample rate. So update device sample rate
			 * and propagate sample rate change event to active
			 * sessions of the device.
			 */
			if (info->set_sample_rate != set_freq) {
				info->set_sample_rate = set_freq;
				if (info->opened) {
					/* Ignore propagating sample rate
					 * change event to requested client
					 * session
					 */
					if (clnt_type == AUDDEV_CLNT_DEC)
						routing_info.\
						dec_freq[session_id].evt = 1;
					else if (clnt_type == AUDDEV_CLNT_ENC)
						routing_info.\
						enc_freq[session_id].evt = 1;
					broadcast_event(AUDDEV_EVT_FREQ_CHG, i,
								SESSION_IGNORE);
					set_freq = info->dev_ops.set_freq(info,
								set_freq);
					broadcast_event(AUDDEV_EVT_DEV_RDY, i,
								SESSION_IGNORE);
				}
			}
		}
		MM_DBG("info->set_sample_rate = %d\n", info->set_sample_rate);
		MM_DBG("routing_info.enc_freq.freq = %d\n",
					routing_info.enc_freq[session_id].freq);
	}
	return rc;
}
EXPORT_SYMBOL(msm_snddev_request_freq);

int msm_snddev_enable_sidetone(u32 dev_id, u32 enable)
{
	int rc;
	struct msm_snddev_info *dev_info;

	MM_DBG("dev_id %d enable %d\n", dev_id, enable);

	dev_info = audio_dev_ctrl_find_dev(dev_id);

	if (IS_ERR(dev_info)) {
		MM_ERR("bad dev_id %d\n", dev_id);
		rc = -EINVAL;
	} else if (!dev_info->dev_ops.enable_sidetone) {
		MM_DBG("dev %d no sidetone support\n", dev_id);
		rc = -EPERM;
	} else
		rc = dev_info->dev_ops.enable_sidetone(dev_info, enable);

	return rc;
}
EXPORT_SYMBOL(msm_snddev_enable_sidetone);

static long audio_dev_ctrl_ioctl(struct file *file,
				 unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	struct audio_dev_ctrl_state *dev_ctrl = file->private_data;

	mutex_lock(&session_lock);
	switch (cmd) {
	case AUDIO_GET_NUM_SND_DEVICE:
		rc = put_user(dev_ctrl->num_dev, (uint32_t __user *) arg);
		break;
	case AUDIO_GET_SND_DEVICES:
		rc = audio_dev_ctrl_get_devices(dev_ctrl, (void __user *) arg);
		break;
	case AUDIO_ENABLE_SND_DEVICE: {
		struct msm_snddev_info *dev_info;
		u32 dev_id;

		if (get_user(dev_id, (u32 __user *) arg)) {
			rc = -EFAULT;
			break;
		}
		dev_info = audio_dev_ctrl_find_dev(dev_id);
		if (IS_ERR(dev_info))
			rc = PTR_ERR(dev_info);
		else {
			rc = dev_info->dev_ops.open(dev_info);
			if (!rc)
				dev_info->opened = 1;
			wake_up(&audio_dev_ctrl.wait);
		}
		break;

	}

	case AUDIO_DISABLE_SND_DEVICE: {
		struct msm_snddev_info *dev_info;
		u32 dev_id;

		if (get_user(dev_id, (u32 __user *) arg)) {
			rc = -EFAULT;
			break;
		}
		dev_info = audio_dev_ctrl_find_dev(dev_id);
		if (IS_ERR(dev_info))
			rc = PTR_ERR(dev_info);
		else {
			rc = dev_info->dev_ops.close(dev_info);
			dev_info->opened = 0;
		}
		break;
	}

	case AUDIO_ROUTE_STREAM: {
		struct msm_audio_route_config route_cfg;
		struct msm_snddev_info *dev_info;

		if (copy_from_user(&route_cfg, (void __user *) arg,
			sizeof(struct msm_audio_route_config))) {
			rc = -EFAULT;
			break;
		}
		MM_DBG("%s: route cfg %d %d type\n", __func__,
		route_cfg.dev_id, route_cfg.stream_type);
		dev_info = audio_dev_ctrl_find_dev(route_cfg.dev_id);
		if (IS_ERR(dev_info)) {
			MM_ERR("%s: pass invalid dev_id\n", __func__);
			rc = PTR_ERR(dev_info);
			break;
		}

		switch (route_cfg.stream_type) {

		case AUDIO_ROUTE_STREAM_VOICE_RX:
			if (!(dev_info->capability & SNDDEV_CAP_RX) |
			    !(dev_info->capability & SNDDEV_CAP_VOICE)) {
				rc = -EINVAL;
				break;
			}
			dev_ctrl->voice_rx_dev = dev_info;
			break;
		case AUDIO_ROUTE_STREAM_VOICE_TX:
			if (!(dev_info->capability & SNDDEV_CAP_TX) |
			    !(dev_info->capability & SNDDEV_CAP_VOICE)) {
				rc = -EINVAL;
				break;
			}
			dev_ctrl->voice_tx_dev = dev_info;
			break;
		}
		break;
	}

	default:
		rc = -EINVAL;
	}
	mutex_unlock(&session_lock);
	return rc;
}

static int audio_dev_ctrl_open(struct inode *inode, struct file *file)
{
	MM_DBG("open audio_dev_ctrl\n");
	atomic_inc(&audio_dev_ctrl.opened);
	file->private_data = &audio_dev_ctrl;
	return 0;
}

static int audio_dev_ctrl_release(struct inode *inode, struct file *file)
{
	MM_DBG("release audio_dev_ctrl\n");
	atomic_dec(&audio_dev_ctrl.opened);
	return 0;
}

static const struct file_operations audio_dev_ctrl_fops = {
	.owner = THIS_MODULE,
	.open = audio_dev_ctrl_open,
	.release = audio_dev_ctrl_release,
	.unlocked_ioctl = audio_dev_ctrl_ioctl,
};


struct miscdevice audio_dev_ctrl_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_audio_dev_ctrl",
	.fops	= &audio_dev_ctrl_fops,
};

/* session id is 32 bit routing mask per device
 * 0-7 for voice clients
 * 8-15 for Decoder clients
 * 16-23 for Encoder clients
 * 24-31 Do not care
 */
void broadcast_event(u32 evt_id, u32 dev_id, u32 session_id)
{
	int clnt_id = 0, i;
	union auddev_evt_data *evt_payload;
	struct msm_snd_evt_listner *callback;
	struct msm_snddev_info *dev_info = NULL;
	u32 session_mask = 0;
	static int pending_sent;

	MM_DBG(": evt_id = %d\n", evt_id);

	if ((evt_id != AUDDEV_EVT_START_VOICE)
		&& (evt_id != AUDDEV_EVT_END_VOICE)
		&& (evt_id != AUDDEV_EVT_STREAM_VOL_CHG)
		&& (evt_id != AUDDEV_EVT_VOICE_STATE_CHG)) {
		dev_info = audio_dev_ctrl_find_dev(dev_id);
		if (IS_ERR(dev_info)) {
			MM_ERR("pass invalid dev_id\n");
			return;
		}
	}

	if (event.cb != NULL)
		callback = event.cb;
	else
		return;

	evt_payload = kzalloc(sizeof(union auddev_evt_data),
			GFP_KERNEL);
	if (evt_payload == NULL) {
		MM_ERR("Memory allocation for event payload failed\n");
		return;
	}

	mutex_lock(&session_lock);

	if (evt_id == AUDDEV_EVT_VOICE_STATE_CHG)
		routing_info.voice_state = dev_id;

	for (; ;) {
		if (!(evt_id & callback->evt_id)) {
			if (callback->cb_next == NULL)
				break;
			else {
				callback = callback->cb_next;
				continue;
			}
		}
		clnt_id = callback->clnt_id;
		memset(evt_payload, 0, sizeof(union auddev_evt_data));

		if ((evt_id == AUDDEV_EVT_START_VOICE)
			|| (evt_id == AUDDEV_EVT_END_VOICE))
			goto skip_check;
		if (callback->clnt_type == AUDDEV_CLNT_AUDIOCAL)
			goto aud_cal;

		session_mask = (0x1 << (clnt_id))
				<< (8 * ((int)callback->clnt_type-1));

		if ((evt_id == AUDDEV_EVT_STREAM_VOL_CHG) || \
			(evt_id == AUDDEV_EVT_VOICE_STATE_CHG)) {
			MM_DBG("AUDDEV_EVT_STREAM_VOL_CHG or\
				AUDDEV_EVT_VOICE_STATE_CHG\n");
			goto volume_strm;
		}

		MM_DBG("dev_info->sessions = %08x\n", dev_info->sessions);

		if ((!session_id && !(dev_info->sessions & session_mask)) ||
			(session_id && ((dev_info->sessions & session_mask) !=
						session_id))) {
			if (callback->cb_next == NULL)
				break;
			else {
				callback = callback->cb_next;
				continue;
			}
		}
		if (evt_id == AUDDEV_EVT_DEV_CHG_VOICE)
			goto voc_events;

volume_strm:
		if (callback->clnt_type == AUDDEV_CLNT_DEC) {
			MM_DBG("AUDDEV_CLNT_DEC\n");
			if (evt_id == AUDDEV_EVT_STREAM_VOL_CHG) {
				MM_DBG("clnt_id = %d, session_id = 0x%8x\n",
					clnt_id, session_id);
				if (session_mask != session_id)
					goto sent_dec;
				else
					evt_payload->session_vol =
						msm_vol_ctl.volume;
			} else if (evt_id == AUDDEV_EVT_FREQ_CHG) {
				if (routing_info.dec_freq[clnt_id].evt) {
					routing_info.dec_freq[clnt_id].evt
							= 0;
					goto sent_dec;
				} else if (routing_info.dec_freq[clnt_id].freq
					== dev_info->set_sample_rate)
					goto sent_dec;
				else {
					evt_payload->freq_info.sample_rate
						= dev_info->set_sample_rate;
					evt_payload->freq_info.dev_type
						= dev_info->capability;
					evt_payload->freq_info.acdb_dev_id
						= dev_info->acdb_id;
				}
			/* Propogate device information to client */
			} else if (evt_id == AUDDEV_EVT_DEVICE_INFO) {
				evt_payload->devinfo.dev_id
					= dev_info->copp_id;
				evt_payload->devinfo.acdb_id
					= dev_info->acdb_id;
				evt_payload->devinfo.dev_type =
					(dev_info->capability & SNDDEV_CAP_TX) ?
					SNDDEV_CAP_TX : SNDDEV_CAP_RX;
				evt_payload->devinfo.sample_rate
					= dev_info->sample_rate;
				if (session_id == SESSION_IGNORE)
					evt_payload->devinfo.sessions
					= dev_info->sessions;
				else
					evt_payload->devinfo.sessions
					= session_id;
				evt_payload->devinfo.sessions =
					(evt_payload->devinfo.sessions >>
						((AUDDEV_CLNT_DEC-1) * 8));
			} else if (evt_id == AUDDEV_EVT_VOICE_STATE_CHG)
				evt_payload->voice_state =
					routing_info.voice_state;
			else
				evt_payload->routing_id = dev_info->copp_id;
			callback->auddev_evt_listener(
					evt_id,
					evt_payload,
					callback->private_data);
sent_dec:
			if ((evt_id != AUDDEV_EVT_STREAM_VOL_CHG) &&
				(evt_id != AUDDEV_EVT_VOICE_STATE_CHG))
				routing_info.dec_freq[clnt_id].freq
						= dev_info->set_sample_rate;

			if (callback->cb_next == NULL)
				break;
			else {
				callback = callback->cb_next;
				continue;
			}
		}
		if (callback->clnt_type == AUDDEV_CLNT_ENC) {

			MM_DBG("AUDDEV_CLNT_ENC\n");
			if (evt_id == AUDDEV_EVT_FREQ_CHG) {
				if (routing_info.enc_freq[clnt_id].evt) {
					routing_info.enc_freq[clnt_id].evt
							= 0;
					goto sent_enc;
				 } else {
					evt_payload->freq_info.sample_rate
						= dev_info->set_sample_rate;
					evt_payload->freq_info.dev_type
						= dev_info->capability;
					evt_payload->freq_info.acdb_dev_id
						= dev_info->acdb_id;
				}
			/* Propogate device information to client */
			} else if (evt_id == AUDDEV_EVT_DEVICE_INFO) {
				evt_payload->devinfo.dev_id
					= dev_info->copp_id;
				evt_payload->devinfo.acdb_id
					= dev_info->acdb_id;
				evt_payload->devinfo.dev_type =
					(dev_info->capability & SNDDEV_CAP_TX) ?
					SNDDEV_CAP_TX : SNDDEV_CAP_RX;
				evt_payload->devinfo.sample_rate
					= dev_info->sample_rate;
				if (session_id == SESSION_IGNORE)
					evt_payload->devinfo.sessions
					= dev_info->sessions;
				else
					evt_payload->devinfo.sessions
					= session_id;
				evt_payload->devinfo.sessions =
					(evt_payload->devinfo.sessions >>
						((AUDDEV_CLNT_ENC-1) * 8));
			} else if (evt_id == AUDDEV_EVT_VOICE_STATE_CHG)
				evt_payload->voice_state =
					routing_info.voice_state;
			else
				evt_payload->routing_id = dev_info->copp_id;
			callback->auddev_evt_listener(
					evt_id,
					evt_payload,
					callback->private_data);
sent_enc:
			if (callback->cb_next == NULL)
					break;
			else {
				callback = callback->cb_next;
				continue;
			}
		}
aud_cal:
		if (callback->clnt_type == AUDDEV_CLNT_AUDIOCAL) {
			int temp_sessions;
			MM_DBG("AUDDEV_CLNT_AUDIOCAL\n");
			if (evt_id == AUDDEV_EVT_VOICE_STATE_CHG)
				evt_payload->voice_state =
					routing_info.voice_state;
			else if (!dev_info->sessions)
				goto sent_aud_cal;
			else {
				evt_payload->audcal_info.dev_id =
						dev_info->copp_id;
				evt_payload->audcal_info.acdb_id =
						dev_info->acdb_id;
				evt_payload->audcal_info.dev_type =
					(dev_info->capability & SNDDEV_CAP_TX) ?
					SNDDEV_CAP_TX : SNDDEV_CAP_RX;
				evt_payload->audcal_info.sample_rate =
					dev_info->set_sample_rate ?
					dev_info->set_sample_rate :
					dev_info->sample_rate;
			}
			if (evt_payload->audcal_info.dev_type ==
						SNDDEV_CAP_TX) {
				if (session_id == SESSION_IGNORE)
					temp_sessions = dev_info->sessions;
				else
					temp_sessions = session_id;
				evt_payload->audcal_info.sessions =
					(temp_sessions >>
						((AUDDEV_CLNT_ENC-1) * 8));
			} else {
				if (session_id == SESSION_IGNORE)
					temp_sessions = dev_info->sessions;
				else
					temp_sessions = session_id;
				evt_payload->audcal_info.sessions =
					(temp_sessions >>
						((AUDDEV_CLNT_DEC-1) * 8));
			}
			callback->auddev_evt_listener(
				evt_id,
				evt_payload,
				callback->private_data);

sent_aud_cal:
			if (callback->cb_next == NULL)
				break;
			else {
				callback = callback->cb_next;
				continue;
			}
		}
skip_check:
voc_events:
		if (callback->clnt_type == AUDDEV_CLNT_VOC) {
			MM_DBG("AUDDEV_CLNT_VOC\n");
			if (evt_id == AUDDEV_EVT_DEV_RLS) {
				if (!pending_sent)
					goto sent_voc;
				else
					pending_sent = 0;
			}
			if (evt_id == AUDDEV_EVT_REL_PENDING)
				pending_sent = 1;

			if (evt_id == AUDDEV_EVT_DEVICE_VOL_MUTE_CHG) {
				if (dev_info->capability & SNDDEV_CAP_TX) {
					evt_payload->voc_vm_info.dev_type =
						SNDDEV_CAP_TX;
					evt_payload->voc_vm_info.acdb_dev_id =
						dev_info->acdb_id;
					evt_payload->
					voc_vm_info.dev_vm_val.mute =
						routing_info.tx_mute;
				} else {
					evt_payload->voc_vm_info.dev_type =
						SNDDEV_CAP_RX;
					evt_payload->voc_vm_info.acdb_dev_id =
						dev_info->acdb_id;
					evt_payload->
					voc_vm_info.dev_vm_val.vol =
						routing_info.voice_rx_vol;
				}
			} else if ((evt_id == AUDDEV_EVT_START_VOICE)
					|| (evt_id == AUDDEV_EVT_END_VOICE))
				memset(evt_payload, 0,
					sizeof(union auddev_evt_data));
			else if (evt_id == AUDDEV_EVT_FREQ_CHG) {
				if (routing_info.voice_tx_sample_rate
						!= dev_info->set_sample_rate) {
					routing_info.voice_tx_sample_rate
						= dev_info->set_sample_rate;
					evt_payload->freq_info.sample_rate
						= dev_info->set_sample_rate;
					evt_payload->freq_info.dev_type
						= dev_info->capability;
					evt_payload->freq_info.acdb_dev_id
						= dev_info->acdb_id;
				} else
					goto sent_voc;
			} else if (evt_id == AUDDEV_EVT_VOICE_STATE_CHG)
				evt_payload->voice_state =
						routing_info.voice_state;
			else {
				evt_payload->voc_devinfo.dev_type =
					(dev_info->capability & SNDDEV_CAP_TX) ?
					SNDDEV_CAP_TX : SNDDEV_CAP_RX;
				evt_payload->voc_devinfo.acdb_dev_id =
					dev_info->acdb_id;
				evt_payload->voc_devinfo.dev_sample =
					dev_info->set_sample_rate ?
					dev_info->set_sample_rate :
					dev_info->sample_rate;
				evt_payload->voc_devinfo.dev_id = dev_id;
				if (dev_info->capability & SNDDEV_CAP_RX) {
					for (i = 0; i < VOC_RX_VOL_ARRAY_NUM;
						i++) {
						evt_payload->
						voc_devinfo.max_rx_vol[i] =
						dev_info->max_voc_rx_vol[i];
						evt_payload
						->voc_devinfo.min_rx_vol[i] =
						dev_info->min_voc_rx_vol[i];
					}
				}
			}
			callback->auddev_evt_listener(
				evt_id,
				evt_payload,
				callback->private_data);
			if (evt_id == AUDDEV_EVT_DEV_RLS)
				dev_info->sessions &= ~(0xFF);
sent_voc:
			if (callback->cb_next == NULL)
				break;
			else {
				callback = callback->cb_next;
				continue;
			}
		}
	}
	kfree(evt_payload);
	mutex_unlock(&session_lock);
}
EXPORT_SYMBOL(broadcast_event);


void mixer_post_event(u32 evt_id, u32 id)
{

	MM_DBG("evt_id = %d\n", evt_id);
	switch (evt_id) {
	case AUDDEV_EVT_DEV_CHG_VOICE: /* Called from Voice_route */
		broadcast_event(AUDDEV_EVT_DEV_CHG_VOICE, id, SESSION_IGNORE);
		break;
	case AUDDEV_EVT_DEV_RDY:
		broadcast_event(AUDDEV_EVT_DEV_RDY, id, SESSION_IGNORE);
		break;
	case AUDDEV_EVT_DEV_RLS:
		broadcast_event(AUDDEV_EVT_DEV_RLS, id, SESSION_IGNORE);
		break;
	case AUDDEV_EVT_REL_PENDING:
		broadcast_event(AUDDEV_EVT_REL_PENDING, id, SESSION_IGNORE);
		break;
	case AUDDEV_EVT_DEVICE_VOL_MUTE_CHG:
		broadcast_event(AUDDEV_EVT_DEVICE_VOL_MUTE_CHG, id,
							SESSION_IGNORE);
		break;
	case AUDDEV_EVT_STREAM_VOL_CHG:
		broadcast_event(AUDDEV_EVT_STREAM_VOL_CHG, id,
							SESSION_IGNORE);
		break;
	case AUDDEV_EVT_START_VOICE:
		broadcast_event(AUDDEV_EVT_START_VOICE,
				id, SESSION_IGNORE);
		break;
	case AUDDEV_EVT_END_VOICE:
		broadcast_event(AUDDEV_EVT_END_VOICE,
				id, SESSION_IGNORE);
		break;
	case AUDDEV_EVT_FREQ_CHG:
		broadcast_event(AUDDEV_EVT_FREQ_CHG, id, SESSION_IGNORE);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(mixer_post_event);

static int __init audio_dev_ctrl_init(void)
{
#ifdef CONFIG_DEBUG_FS
	char name[sizeof "rtc_get_device"+1];
#endif

	init_waitqueue_head(&audio_dev_ctrl.wait);

	event.cb = NULL;

	atomic_set(&audio_dev_ctrl.opened, 0);
	audio_dev_ctrl.num_dev = 0;
	audio_dev_ctrl.voice_tx_dev = NULL;
	audio_dev_ctrl.voice_rx_dev = NULL;
	routing_info.voice_state = VOICE_STATE_INVALID;
#ifdef CONFIG_DEBUG_FS
	snprintf(name, sizeof name, "rtc_get_device");
	dentry = debugfs_create_file(name, S_IFREG | S_IRUGO | S_IWUGO,
			NULL, NULL, &rtc_acdb_debug_fops);
	if (IS_ERR(dentry))
		MM_DBG("debugfs_create_file failed\n");
#endif

	return misc_register(&audio_dev_ctrl_misc);
}

static void __exit audio_dev_ctrl_exit(void)
{
#ifdef CONFIG_DEBUG_FS
	if (dentry)
		debugfs_remove(dentry);
#endif

}
module_init(audio_dev_ctrl_init);
module_exit(audio_dev_ctrl_exit);

MODULE_DESCRIPTION("MSM 7K Audio Device Control driver");
MODULE_LICENSE("GPL v2");
