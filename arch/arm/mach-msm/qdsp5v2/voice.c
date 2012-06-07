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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/msm_audio.h>
#include <mach/qdsp5v2/audio_dev_ctl.h>
#include <mach/dal.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/wait.h>
#include <mach/qdsp5v2/voice.h>
#include <mach/debug_mm.h>

struct voice_data {
	void *handle; /* DALRPC handle */
	void *cb_handle; /* DALRPC callback handle */
	int network; /* Network information */
	int dev_state;/*READY, CHANGE, REL_DONE,INIT*/
	int voc_state;/*INIT, CHANGE, RELEASE, ACQUIRE */
	struct mutex voc_lock;
	struct mutex vol_lock;
	int voc_event;
	int dev_event;
	atomic_t rel_start_flag;
	atomic_t acq_start_flag;
	atomic_t chg_start_flag;
	struct task_struct *task;
	struct completion complete;
	wait_queue_head_t dev_wait;
	wait_queue_head_t voc_wait;
	uint32_t device_events;
	/* cache the values related to Rx and Tx */
	struct device_data dev_rx;
	struct device_data dev_tx;
	/* these default values are for all devices */
	uint32_t default_mute_val;
	uint32_t default_vol_val;
	uint32_t default_sample_val;
	/* call status */
	int v_call_status; /* Start or End */
	s32 max_rx_vol[VOC_RX_VOL_ARRAY_NUM]; /* [0] is for NB, [1] for WB */
	s32 min_rx_vol[VOC_RX_VOL_ARRAY_NUM];
};

static struct voice_data voice;

static int voice_cmd_device_info(struct voice_data *);
static int voice_cmd_acquire_done(struct voice_data *);
static void voice_auddev_cb_function(u32 evt_id,
			union auddev_evt_data *evt_payload,
			void *private_data);

static int voice_cmd_change(void)
{

	struct voice_header hdr;
	struct voice_data *v = &voice;
	int err;

	hdr.id = CMD_DEVICE_CHANGE;
	hdr.data_len = 0;

	MM_DBG("\n"); /* Macro prints the file name and function */

	err = dalrpc_fcn_5(VOICE_DALRPC_CMD, v->handle, &hdr,
			sizeof(struct voice_header));

	if (err)
		MM_ERR("Voice change command failed\n");
	return err;
}

static void voice_auddev_cb_function(u32 evt_id,
			union auddev_evt_data *evt_payload,
			void *private_data)
{
	struct voice_data *v = &voice;
	int rc = 0, i;

	MM_INFO("auddev_cb_function, evt_id=%d, dev_state=%d, voc_state=%d\n",
		evt_id, v->dev_state, v->voc_state);
	if ((evt_id != AUDDEV_EVT_START_VOICE) ||
			(evt_id != AUDDEV_EVT_END_VOICE)) {
		if (evt_payload == NULL) {
			MM_ERR(" evt_payload is NULL pointer\n");
			return;
		}
	}
	switch (evt_id) {
	case AUDDEV_EVT_START_VOICE:
		if ((v->dev_state == DEV_INIT) ||
				(v->dev_state == DEV_REL_DONE)) {
			v->v_call_status = VOICE_CALL_START;
			if ((v->dev_rx.enabled == VOICE_DEV_ENABLED)
				&& (v->dev_tx.enabled == VOICE_DEV_ENABLED)) {
				v->dev_state = DEV_READY;
				MM_DBG("dev_state into ready\n");
				wake_up(&v->dev_wait);
			}
			if (v->voc_state == VOICE_CHANGE) {
				MM_DBG("voc_state is in VOICE_CHANGE\n");
				v->voc_state = VOICE_ACQUIRE;
			}
		}
		break;
	case AUDDEV_EVT_DEV_CHG_VOICE:
		if (v->dev_state == DEV_READY) {
			v->dev_rx.enabled = VOICE_DEV_DISABLED;
			v->dev_tx.enabled = VOICE_DEV_DISABLED;
			v->dev_state = DEV_CHANGE;
			mutex_lock(&voice.voc_lock);
			if (v->voc_state == VOICE_ACQUIRE) {
				/* send device change to modem */
				voice_cmd_change();
				mutex_unlock(&voice.voc_lock);
				msm_snddev_enable_sidetone(v->dev_rx.dev_id,
				0);
				/* block to wait for CHANGE_START */
				rc = wait_event_interruptible(
				v->voc_wait, (v->voc_state == VOICE_CHANGE)
				|| (atomic_read(&v->chg_start_flag) == 1)
				|| (atomic_read(&v->rel_start_flag) == 1));
			} else {
				mutex_unlock(&voice.voc_lock);
				MM_ERR(" Voice is not at ACQUIRE state\n");
			}
		} else if ((v->dev_state == DEV_INIT) ||
				(v->dev_state == DEV_REL_DONE)) {
				v->dev_rx.enabled = VOICE_DEV_DISABLED;
				v->dev_tx.enabled = VOICE_DEV_DISABLED;
		} else
			MM_ERR(" device is not at proper state\n");
		break;
	case AUDDEV_EVT_DEV_RDY:
		/* update the dev info */
		if (evt_payload->voc_devinfo.dev_type == DIR_RX) {
			for (i = 0; i < VOC_RX_VOL_ARRAY_NUM; i++) {
				v->max_rx_vol[i] =
					evt_payload->voc_devinfo.max_rx_vol[i];
				v->min_rx_vol[i] =
					evt_payload->voc_devinfo.min_rx_vol[i];
			}
		}
		if (v->dev_state == DEV_CHANGE) {
			if (evt_payload->voc_devinfo.dev_type == DIR_RX) {
				v->dev_rx.dev_acdb_id =
					evt_payload->voc_devinfo.acdb_dev_id;
				v->dev_rx.sample =
					evt_payload->voc_devinfo.dev_sample;
				v->dev_rx.dev_id =
				evt_payload->voc_devinfo.dev_id;
				v->dev_rx.enabled = VOICE_DEV_ENABLED;
			} else {
				v->dev_tx.dev_acdb_id =
					evt_payload->voc_devinfo.acdb_dev_id;
				v->dev_tx.sample =
					evt_payload->voc_devinfo.dev_sample;
				v->dev_tx.enabled = VOICE_DEV_ENABLED;
				v->dev_tx.dev_id =
				evt_payload->voc_devinfo.dev_id;
			}
			if ((v->dev_rx.enabled == VOICE_DEV_ENABLED) &&
				(v->dev_tx.enabled == VOICE_DEV_ENABLED)) {
				v->dev_state = DEV_READY;
				MM_DBG("dev state into ready\n");
				voice_cmd_device_info(v);
				wake_up(&v->dev_wait);
				mutex_lock(&voice.voc_lock);
				if (v->voc_state == VOICE_CHANGE) {
					v->dev_event = DEV_CHANGE_READY;
					complete(&v->complete);
				}
				mutex_unlock(&voice.voc_lock);
			}
		} else if ((v->dev_state == DEV_INIT) ||
			(v->dev_state == DEV_REL_DONE)) {
			if (evt_payload->voc_devinfo.dev_type == DIR_RX) {
				v->dev_rx.dev_acdb_id =
					evt_payload->voc_devinfo.acdb_dev_id;
				v->dev_rx.sample =
					evt_payload->voc_devinfo.dev_sample;
				v->dev_rx.dev_id =
				evt_payload->voc_devinfo.dev_id;
				v->dev_rx.enabled = VOICE_DEV_ENABLED;
			} else {
				v->dev_tx.dev_acdb_id =
					evt_payload->voc_devinfo.acdb_dev_id;
				v->dev_tx.sample =
					evt_payload->voc_devinfo.dev_sample;
				v->dev_tx.dev_id =
				evt_payload->voc_devinfo.dev_id;
				v->dev_tx.enabled = VOICE_DEV_ENABLED;
			}
			if ((v->dev_rx.enabled == VOICE_DEV_ENABLED) &&
				(v->dev_tx.enabled == VOICE_DEV_ENABLED) &&
				(v->v_call_status == VOICE_CALL_START)) {
				v->dev_state = DEV_READY;
				MM_DBG("dev state into ready\n");
				voice_cmd_device_info(v);
				wake_up(&v->dev_wait);
				mutex_lock(&voice.voc_lock);
				if (v->voc_state == VOICE_CHANGE) {
					v->dev_event = DEV_CHANGE_READY;
					complete(&v->complete);
				}
				mutex_unlock(&voice.voc_lock);
			}
		} else
			MM_ERR("Receive READY not at the proper state =%d\n",
				v->dev_state);
		break;
	case AUDDEV_EVT_DEVICE_VOL_MUTE_CHG:
		if (evt_payload->voc_devinfo.dev_type == DIR_TX)
			v->dev_tx.mute =
				evt_payload->voc_vm_info.dev_vm_val.mute;
		else
			v->dev_rx.volume = evt_payload->
						voc_vm_info.dev_vm_val.vol;
		/* send device info */
		voice_cmd_device_info(v);
		break;
	case AUDDEV_EVT_REL_PENDING:
		/* recover the tx mute and rx volume to the default values */
		if (v->dev_state == DEV_READY) {
			if (atomic_read(&v->rel_start_flag)) {
				atomic_dec(&v->rel_start_flag);
				if (evt_payload->voc_devinfo.dev_type == DIR_RX)
					v->dev_rx.enabled = VOICE_DEV_DISABLED;
				else
					v->dev_tx.enabled = VOICE_DEV_DISABLED;
				v->dev_state = DEV_REL_DONE;
				wake_up(&v->dev_wait);
				break;
			}
			mutex_lock(&voice.voc_lock);
			if ((v->voc_state == VOICE_RELEASE) ||
					(v->voc_state == VOICE_INIT)) {
				if (evt_payload->voc_devinfo.dev_type
							== DIR_RX) {
					v->dev_rx.enabled = VOICE_DEV_DISABLED;
				} else {
					v->dev_tx.enabled = VOICE_DEV_DISABLED;
				}
				v->dev_state = DEV_REL_DONE;
				mutex_unlock(&voice.voc_lock);
				wake_up(&v->dev_wait);
			} else {
				/* send device change to modem */
				voice_cmd_change();
				mutex_unlock(&voice.voc_lock);
				rc = wait_event_interruptible(
				v->voc_wait, (v->voc_state == VOICE_CHANGE)
				|| (atomic_read(&v->chg_start_flag) == 1)
				|| (atomic_read(&v->rel_start_flag) == 1));
				if (atomic_read(&v->rel_start_flag) == 1)
					atomic_dec(&v->rel_start_flag);
				/* clear Rx/Tx to Disable */
				if (evt_payload->voc_devinfo.dev_type == DIR_RX)
					v->dev_rx.enabled = VOICE_DEV_DISABLED;
				else
					v->dev_tx.enabled = VOICE_DEV_DISABLED;
				v->dev_state = DEV_REL_DONE;
				wake_up(&v->dev_wait);
			}
		} else if ((v->dev_state == DEV_INIT) ||
				(v->dev_state == DEV_REL_DONE)) {
			if (evt_payload->voc_devinfo.dev_type == DIR_RX)
				v->dev_rx.enabled = VOICE_DEV_DISABLED;
			else
				v->dev_tx.enabled = VOICE_DEV_DISABLED;
		}
		break;
	case AUDDEV_EVT_END_VOICE:
		/* recover the tx mute and rx volume to the default values */
		v->dev_tx.mute = v->default_mute_val;
		v->dev_rx.volume = v->default_vol_val;

		if (v->dev_rx.enabled == VOICE_DEV_ENABLED)
			msm_snddev_enable_sidetone(v->dev_rx.dev_id, 0);

		if ((v->dev_state == DEV_READY) ||
			(v->dev_state == DEV_CHANGE)) {
			if (atomic_read(&v->rel_start_flag)) {
				atomic_dec(&v->rel_start_flag);
				v->v_call_status = VOICE_CALL_END;
				v->dev_state = DEV_REL_DONE;
				wake_up(&v->dev_wait);
				break;
			}
			mutex_lock(&voice.voc_lock);
			if ((v->voc_state == VOICE_RELEASE) ||
					(v->voc_state == VOICE_INIT)) {
				v->v_call_status = VOICE_CALL_END;
				v->dev_state = DEV_REL_DONE;
				mutex_unlock(&voice.voc_lock);
				wake_up(&v->dev_wait);
			} else {
				/* send mute and default volume value to MCAD */
				voice_cmd_device_info(v);
				/* send device change to modem */
				voice_cmd_change();
				mutex_unlock(&voice.voc_lock);
				/* block to wait for RELEASE_START
						or CHANGE_START */
				rc = wait_event_interruptible(
				v->voc_wait, (v->voc_state == VOICE_CHANGE)
				|| (atomic_read(&v->chg_start_flag) == 1)
				|| (atomic_read(&v->rel_start_flag) == 1));
				if (atomic_read(&v->rel_start_flag) == 1)
					atomic_dec(&v->rel_start_flag);
				/* set voice call to END state */
				v->v_call_status = VOICE_CALL_END;
				v->dev_state = DEV_REL_DONE;
				wake_up(&v->dev_wait);
			}
		} else
			v->v_call_status = VOICE_CALL_END;
		break;
	case AUDDEV_EVT_FREQ_CHG:
		MM_DBG("Voice Driver got sample rate change Event\n");
		MM_DBG("sample rate %d\n", evt_payload->freq_info.sample_rate);
		MM_DBG("dev_type %d\n", evt_payload->freq_info.dev_type);
		MM_DBG("acdb_dev_id %d\n", evt_payload->freq_info.acdb_dev_id);
		if (v->dev_state == DEV_READY) {
			v->dev_tx.enabled = VOICE_DEV_DISABLED;
			v->dev_state = DEV_CHANGE;
			mutex_lock(&voice.voc_lock);
			if (v->voc_state == VOICE_ACQUIRE) {
				msm_snddev_enable_sidetone(v->dev_rx.dev_id,
				0);
				/* send device change to modem */
				voice_cmd_change();
				mutex_unlock(&voice.voc_lock);
				/* block to wait for CHANGE_START */
				rc = wait_event_interruptible(
				v->voc_wait, (v->voc_state == VOICE_CHANGE)
				|| (atomic_read(&v->chg_start_flag) == 1)
				|| (atomic_read(&v->rel_start_flag) == 1));
			} else {
				mutex_unlock(&voice.voc_lock);
				MM_ERR(" Voice is not at ACQUIRE state\n");
			}
		} else if ((v->dev_state == DEV_INIT) ||
				(v->dev_state == DEV_REL_DONE)) {
				v->dev_tx.enabled = VOICE_DEV_DISABLED;
		} else
			MM_ERR("Event not at the proper state =%d\n",
				v->dev_state);
		break;
	default:
		MM_ERR("UNKNOWN EVENT\n");
	}
	return;
}
EXPORT_SYMBOL(voice_auddev_cb_function);

static void remote_cb_function(void *context, u32 param,
				void *evt_buf, u32 len)
{
	struct voice_header *hdr;
	struct voice_data *v = context;

	hdr = (struct voice_header *)evt_buf;

	MM_INFO("len=%d id=%d\n", len, hdr->id);

	if (len <= 0) {
		MM_ERR("unexpected event with length %d \n", len);
		return;
	}

	switch (hdr->id) {
	case EVENT_ACQUIRE_START:
		atomic_inc(&v->acq_start_flag);
		wake_up(&v->dev_wait);
		v->voc_event = VOICE_ACQUIRE_START;
		v->network = ((struct voice_network *)evt_buf)->network_info;
		complete(&v->complete);
		break;
	case EVENT_RELEASE_START:
		/* If ACQUIRED come in before the RELEASE,
		* will only services the RELEASE */
		atomic_inc(&v->rel_start_flag);
		wake_up(&v->voc_wait);
		wake_up(&v->dev_wait);
		v->voc_event = VOICE_RELEASE_START;
		complete(&v->complete);
		break;
	case EVENT_CHANGE_START:
		atomic_inc(&v->chg_start_flag);
		wake_up(&v->voc_wait);
		v->voc_event = VOICE_CHANGE_START;
		complete(&v->complete);
		break;
	case EVENT_NETWORK_RECONFIG:
		/* send network change to audio_dev,
		if sample rate is less than 16k,
		otherwise, send acquire done */
		v->voc_event = VOICE_NETWORK_RECONFIG;
		v->network = ((struct voice_network *)evt_buf)->network_info;
		complete(&v->complete);
		break;
	default:
		MM_ERR("Undefined event %d \n", hdr->id);
	}

}

static int voice_cmd_init(struct voice_data *v)
{

	struct voice_init cmd;
	int err;

	MM_DBG("\n"); /* Macro prints the file name and function */

	cmd.hdr.id = CMD_VOICE_INIT;
	cmd.hdr.data_len = sizeof(struct voice_init) -
				sizeof(struct voice_header);
	cmd.cb_handle = v->cb_handle;

	err = dalrpc_fcn_5(VOICE_DALRPC_CMD, v->handle, &cmd,
			 sizeof(struct voice_init));

	if (err)
		MM_ERR("Voice init command failed\n");
	return err;
}

static int voice_cmd_acquire_done(struct voice_data *v)
{
	struct voice_header hdr;
	int err;

	hdr.id = CMD_ACQUIRE_DONE;
	hdr.data_len = 0;

	MM_INFO("\n"); /* Macro prints the file name and function */

	/* Enable HW sidetone if device supports it  */
	msm_snddev_enable_sidetone(v->dev_rx.dev_id, 1);

	err = dalrpc_fcn_5(VOICE_DALRPC_CMD, v->handle, &hdr,
			 sizeof(struct voice_header));

	if (err)
		MM_ERR("Voice acquire done command failed\n");
	return err;
}

static int voice_cmd_device_info(struct voice_data *v)
{
	struct voice_device cmd;
	int err, vol;

	MM_INFO("tx_dev=%d, rx_dev=%d, tx_sample=%d, tx_mute=%d\n",
			v->dev_tx.dev_acdb_id, v->dev_rx.dev_acdb_id,
			v->dev_tx.sample, v->dev_tx.mute);

	mutex_lock(&voice.vol_lock);

	cmd.hdr.id = CMD_DEVICE_INFO;
	cmd.hdr.data_len = sizeof(struct voice_device) -
			sizeof(struct voice_header);
	cmd.tx_device = v->dev_tx.dev_acdb_id;
	cmd.rx_device = v->dev_rx.dev_acdb_id;
	if (v->network == NETWORK_WCDMA_WB)
		vol = v->min_rx_vol[VOC_WB_INDEX] +
			((v->max_rx_vol[VOC_WB_INDEX] -
			v->min_rx_vol[VOC_WB_INDEX]) * v->dev_rx.volume)/100;
	else
		vol = v->min_rx_vol[VOC_NB_INDEX] +
			((v->max_rx_vol[VOC_NB_INDEX] -
			v->min_rx_vol[VOC_NB_INDEX]) * v->dev_rx.volume)/100;
	cmd.rx_volume = (u32)vol; /* in mb */
	cmd.rx_mute = 0;
	cmd.tx_mute = v->dev_tx.mute;
	cmd.rx_sample = v->dev_rx.sample/1000;
	cmd.tx_sample = v->dev_tx.sample/1000;

	MM_DBG("rx_vol=%d, rx_sample=%d\n", cmd.rx_volume, v->dev_rx.sample);

	err = dalrpc_fcn_5(VOICE_DALRPC_CMD, v->handle, &cmd,
			 sizeof(struct voice_device));

	mutex_unlock(&voice.vol_lock);

	if (err)
		MM_ERR("Voice device command failed\n");
	return err;
}
EXPORT_SYMBOL(voice_cmd_device_info);

void voice_change_sample_rate(struct voice_data *v)
{
	int freq = 48000;
	int rc = 0;

	MM_DBG("network =%d, vote freq=%d\n", v->network, freq);
	if (freq != v->dev_tx.sample) {
		rc = msm_snddev_request_freq(&freq, 0,
				SNDDEV_CAP_TX, AUDDEV_CLNT_VOC);
		if (rc >= 0) {
			v->dev_tx.sample = freq;
			MM_DBG(" vote for freq=%d successfully \n", freq);
		} else
			MM_ERR(" voting for freq=%d failed.\n", freq);
	}
}

static int voice_thread(void *data)
{
	struct voice_data *v = (struct voice_data *)data;
	int rc = 0;

	MM_INFO("voice_thread() start\n");

	while (!kthread_should_stop()) {
		wait_for_completion(&v->complete);
		init_completion(&v->complete);

		MM_DBG(" voc_event=%d, voice state =%d, dev_event=%d\n",
				v->voc_event, v->voc_state, v->dev_event);
		switch (v->voc_event) {
		case VOICE_ACQUIRE_START:
			/* check if dev_state = READY */
			/* if ready, send device_info and acquire_done */
			/* if not ready, block to wait the dev_state = READY */
			if ((v->voc_state == VOICE_INIT) ||
				(v->voc_state == VOICE_RELEASE)) {
				if (v->dev_state == DEV_READY) {
					mutex_lock(&voice.voc_lock);
					voice_change_sample_rate(v);
					rc = voice_cmd_device_info(v);
					rc = voice_cmd_acquire_done(v);
					v->voc_state = VOICE_ACQUIRE;
					mutex_unlock(&voice.voc_lock);
					broadcast_event(
					AUDDEV_EVT_VOICE_STATE_CHG,
					VOICE_STATE_INCALL, SESSION_IGNORE);
				} else {
					rc = wait_event_interruptible(
					v->dev_wait,
					(v->dev_state == DEV_READY)
					|| (atomic_read(&v->rel_start_flag)
						== 1));
					if (atomic_read(&v->rel_start_flag)
						== 1) {
						v->voc_state = VOICE_RELEASE;
						atomic_dec(&v->rel_start_flag);
						msm_snddev_withdraw_freq(0,
						SNDDEV_CAP_TX, AUDDEV_CLNT_VOC);
						broadcast_event(
						AUDDEV_EVT_VOICE_STATE_CHG,
						VOICE_STATE_OFFCALL,
						SESSION_IGNORE);
					} else {
						mutex_lock(&voice.voc_lock);
						voice_change_sample_rate(v);
						rc = voice_cmd_device_info(v);
						rc = voice_cmd_acquire_done(v);
						v->voc_state = VOICE_ACQUIRE;
						mutex_unlock(&voice.voc_lock);
						broadcast_event(
						AUDDEV_EVT_VOICE_STATE_CHG,
						VOICE_STATE_INCALL,
						SESSION_IGNORE);
					}
				}
			} else
				MM_ERR("Get this event at the wrong state\n");
			if (atomic_read(&v->acq_start_flag))
				atomic_dec(&v->acq_start_flag);
			break;
		case VOICE_RELEASE_START:
			MM_DBG("broadcast voice call end\n");
			broadcast_event(AUDDEV_EVT_VOICE_STATE_CHG,
					VOICE_STATE_OFFCALL, SESSION_IGNORE);
			if ((v->dev_state == DEV_REL_DONE) ||
					(v->dev_state == DEV_INIT)) {
				v->voc_state = VOICE_RELEASE;
				msm_snddev_withdraw_freq(0, SNDDEV_CAP_TX,
					AUDDEV_CLNT_VOC);
			} else {
				/* wait for the dev_state = RELEASE */
				rc = wait_event_interruptible(v->dev_wait,
					(v->dev_state == DEV_REL_DONE)
				|| (atomic_read(&v->acq_start_flag) == 1));
				if (atomic_read(&v->acq_start_flag) == 1)
					atomic_dec(&v->acq_start_flag);
				v->voc_state = VOICE_RELEASE;
				msm_snddev_withdraw_freq(0, SNDDEV_CAP_TX,
					AUDDEV_CLNT_VOC);
			}
			if (atomic_read(&v->rel_start_flag))
				atomic_dec(&v->rel_start_flag);
			break;
		case VOICE_CHANGE_START:
			if (v->voc_state == VOICE_ACQUIRE)
				v->voc_state = VOICE_CHANGE;
			else
				MM_ERR("Get this event at the wrong state\n");
			wake_up(&v->voc_wait);
			if (atomic_read(&v->chg_start_flag))
				atomic_dec(&v->chg_start_flag);
			break;
		case VOICE_NETWORK_RECONFIG:
			if ((v->voc_state == VOICE_ACQUIRE)
				|| (v->voc_state == VOICE_CHANGE)) {
				voice_change_sample_rate(v);
				rc = voice_cmd_device_info(v);
				rc = voice_cmd_acquire_done(v);
			}
			break;
		default:
			break;
		}

		switch (v->dev_event) {
		case DEV_CHANGE_READY:
			if (v->voc_state == VOICE_CHANGE) {
				mutex_lock(&voice.voc_lock);
				msm_snddev_enable_sidetone(v->dev_rx.dev_id,
				1);
				/* update voice state */
				v->voc_state = VOICE_ACQUIRE;
				v->dev_event = 0;
				mutex_unlock(&voice.voc_lock);
				broadcast_event(AUDDEV_EVT_VOICE_STATE_CHG,
					VOICE_STATE_INCALL, SESSION_IGNORE);
			} else {
				mutex_lock(&voice.voc_lock);
				v->dev_event = 0;
				mutex_unlock(&voice.voc_lock);
				MM_ERR("Get this event at the wrong state\n");
			}
			break;
		default:
			mutex_lock(&voice.voc_lock);
			v->dev_event = 0;
			mutex_unlock(&voice.voc_lock);
			break;
		}
	}
	return 0;
}

static int __init voice_init(void)
{
	int rc, i;
	struct voice_data *v = &voice;
	MM_INFO("\n"); /* Macro prints the file name and function */

	mutex_init(&voice.voc_lock);
	mutex_init(&voice.vol_lock);
	v->handle = NULL;
	v->cb_handle = NULL;

	/* set default value */
	v->default_mute_val = 1;  /* default is mute */
	v->default_vol_val = 0;
	v->default_sample_val = 8000;
	for (i = 0; i < VOC_RX_VOL_ARRAY_NUM; i++) {
		v->max_rx_vol[i] = 0;
		v->min_rx_vol[i] = 0;
	}
	v->network = NETWORK_GSM;

	/* initialize dev_rx and dev_tx */
	memset(&v->dev_tx, 0, sizeof(struct device_data));
	memset(&v->dev_rx, 0, sizeof(struct device_data));
	v->dev_rx.volume = v->default_vol_val;
	v->dev_tx.mute = v->default_mute_val;

	v->dev_state = DEV_INIT;
	v->voc_state = VOICE_INIT;
	atomic_set(&v->rel_start_flag, 0);
	atomic_set(&v->acq_start_flag, 0);
	v->dev_event = 0;
	v->voc_event = 0;
	init_completion(&voice.complete);
	init_waitqueue_head(&v->dev_wait);
	init_waitqueue_head(&v->voc_wait);

	 /* get device handle */
	rc = daldevice_attach(VOICE_DALRPC_DEVICEID,
				VOICE_DALRPC_PORT_NAME,
				VOICE_DALRPC_CPU,
				&v->handle);
	if (rc) {
		MM_ERR("Voc DALRPC call to Modem attach failed\n");
		goto done;
	}

	/* Allocate the callback handle */
	v->cb_handle = dalrpc_alloc_cb(v->handle, remote_cb_function, v);
	if (v->cb_handle == NULL) {
		MM_ERR("Allocate Callback failure\n");
		goto err;
	}

	/* setup the callback */
	rc = voice_cmd_init(v);
	if (rc)
		goto err1;

	v->device_events = AUDDEV_EVT_DEV_CHG_VOICE |
			AUDDEV_EVT_DEV_RDY |
			AUDDEV_EVT_REL_PENDING |
			AUDDEV_EVT_START_VOICE |
			AUDDEV_EVT_END_VOICE |
			AUDDEV_EVT_DEVICE_VOL_MUTE_CHG |
			AUDDEV_EVT_FREQ_CHG;

	MM_DBG(" to register call back \n");
	/* register callback to auddev */
	auddev_register_evt_listner(v->device_events, AUDDEV_CLNT_VOC,
				0, voice_auddev_cb_function, v);

	/* create and start thread */
	v->task = kthread_run(voice_thread, v, "voice");
	if (IS_ERR(v->task)) {
		rc = PTR_ERR(v->task);
		v->task = NULL;
	} else
		goto done;

err1:   dalrpc_dealloc_cb(v->handle, v->cb_handle);
err:
	daldevice_detach(v->handle);
	v->handle = NULL;
done:
	return rc;
}

late_initcall(voice_init);
