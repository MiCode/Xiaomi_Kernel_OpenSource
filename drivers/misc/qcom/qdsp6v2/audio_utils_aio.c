/* Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2009-2015, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <asm/ioctls.h>
#include <linux/debugfs.h>
#include <linux/msm_audio_ion.h>
#include <linux/compat.h>
#include "audio_utils_aio.h"
#ifdef CONFIG_USE_DEV_CTRL_VOLUME
#include <linux/qdsp6v2/audio_dev_ctl.h>
#endif /*CONFIG_USE_DEV_CTRL_VOLUME*/
#ifdef CONFIG_DEBUG_FS
int audio_aio_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

ssize_t audio_aio_debug_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	const int debug_bufmax = 4096;
	static char buffer[4096];
	int n = 0;
	struct q6audio_aio *audio = file->private_data;

	mutex_lock(&audio->lock);
	n = scnprintf(buffer, debug_bufmax, "opened %d\n", audio->opened);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"enabled %d\n", audio->enabled);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"stopped %d\n", audio->stopped);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"feedback %d\n", audio->feedback);
	mutex_unlock(&audio->lock);
	/* Following variables are only useful for debugging when
	 * when playback halts unexpectedly. Thus, no mutual exclusion
	 * enforced
	 */
	n += scnprintf(buffer + n, debug_bufmax - n,
			"wflush %d\n", audio->wflush);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"rflush %d\n", audio->rflush);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"inqueue empty %d\n", list_empty(&audio->in_queue));
	n += scnprintf(buffer + n, debug_bufmax - n,
			"outqueue empty %d\n", list_empty(&audio->out_queue));
	buffer[n] = 0;
	return simple_read_from_buffer(buf, count, ppos, buffer, n);
}
#endif

static long audio_aio_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg);
#ifdef CONFIG_COMPAT
static long audio_aio_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg);
#else
#define audio_aio_compat_ioctl NULL
#endif
int insert_eos_buf(struct q6audio_aio *audio,
		struct audio_aio_buffer_node *buf_node)
{
	struct dec_meta_out *eos_buf = buf_node->kvaddr;
	pr_debug("%s[%p]:insert_eos_buf\n", __func__, audio);
	eos_buf->num_of_frames = 0xFFFFFFFF;
	eos_buf->meta_out_dsp[0].offset_to_frame = 0x0;
	eos_buf->meta_out_dsp[0].nflags = AUDIO_DEC_EOS_SET;
	return sizeof(struct dec_meta_out) +
		sizeof(eos_buf->meta_out_dsp[0]);
}

/* Routine which updates read buffers of driver/dsp,
   for flush operation as DSP output might not have proper
   value set */
static int insert_meta_data_flush(struct q6audio_aio *audio,
	struct audio_aio_buffer_node *buf_node)
{
	struct dec_meta_out *meta_data = buf_node->kvaddr;
	meta_data->num_of_frames = 0x0;
	meta_data->meta_out_dsp[0].offset_to_frame = 0x0;
	meta_data->meta_out_dsp[0].nflags = 0x0;
	return sizeof(struct dec_meta_out) +
		sizeof(meta_data->meta_out_dsp[0]);
}

static int audio_aio_ion_lookup_vaddr(struct q6audio_aio *audio, void *addr,
					unsigned long len,
					struct audio_aio_ion_region **region)
{
	struct audio_aio_ion_region *region_elt;

	int match_count = 0;

	*region = NULL;

	/* returns physical address or zero */
	list_for_each_entry(region_elt, &audio->ion_region_queue, list) {
		if (addr >= region_elt->vaddr &&
			addr < region_elt->vaddr + region_elt->len &&
			addr + len <= region_elt->vaddr + region_elt->len) {
			/* offset since we could pass vaddr inside a registerd
			* ion buffer
			*/

			match_count++;
			if (!*region)
				*region = region_elt;
		}
	}

	if (match_count > 1) {
		pr_err("%s[%p]:multiple hits for vaddr %p, len %ld\n",
			__func__, audio, addr, len);
		list_for_each_entry(region_elt, &audio->ion_region_queue,
					list) {
			if (addr >= region_elt->vaddr &&
			addr < region_elt->vaddr + region_elt->len &&
			addr + len <= region_elt->vaddr + region_elt->len)
				pr_err("\t%s[%p]:%p, %ld --> %pa\n",
					__func__, audio,
					region_elt->vaddr,
					region_elt->len,
					&region_elt->paddr);
		}
	}

	return *region ? 0 : -1;
}

static phys_addr_t audio_aio_ion_fixup(struct q6audio_aio *audio, void *addr,
				unsigned long len, int ref_up, void **kvaddr)
{
	struct audio_aio_ion_region *region;
	phys_addr_t paddr;
	int ret;

	ret = audio_aio_ion_lookup_vaddr(audio, addr, len, &region);
	if (ret) {
		pr_err("%s[%p]:lookup (%p, %ld) failed\n",
				__func__, audio, addr, len);
		return 0;
	}
	if (ref_up)
		region->ref_cnt++;
	else
		region->ref_cnt--;
	pr_debug("%s[%p]:found region %p ref_cnt %d\n",
			__func__, audio, region, region->ref_cnt);
	paddr = region->paddr + (addr - region->vaddr);
	/* provide kernel virtual address for accessing meta information */
	if (kvaddr)
		*kvaddr = (void *) (region->kvaddr + (addr - region->vaddr));
	return paddr;
}

static int audio_aio_pause(struct q6audio_aio  *audio)
{
	int rc = -EINVAL;

	pr_debug("%s[%p], enabled = %d\n", __func__, audio,
			audio->enabled);
	if (audio->enabled) {
		rc = q6asm_cmd(audio->ac, CMD_PAUSE);
		if (rc < 0)
			pr_err("%s[%p]: pause cmd failed rc=%d\n",
				__func__, audio, rc);

		if (rc == 0) {
			/* Send suspend only if pause was successful */
			rc = q6asm_cmd(audio->ac, CMD_SUSPEND);
			if (rc < 0)
				pr_err("%s[%p]: suspend cmd failed rc=%d\n",
					__func__, audio, rc);
		} else
			pr_err("%s[%p]: not sending suspend since pause failed\n",
				__func__, audio);

	} else
		pr_err("%s[%p]: Driver not enabled\n", __func__, audio);
	return rc;
}

static int audio_aio_flush(struct q6audio_aio  *audio)
{
	int rc;

	if (audio->enabled) {
		/* Implicitly issue a pause to the decoder before flushing if
		   it is not in pause state */
		if (!(audio->drv_status & ADRV_STATUS_PAUSE)) {
			rc = audio_aio_pause(audio);
			if (rc < 0)
				pr_err("%s[%p}: pause cmd failed rc=%d\n",
					__func__, audio,
					rc);
			else
				audio->drv_status |= ADRV_STATUS_PAUSE;
		}
		rc = q6asm_cmd(audio->ac, CMD_FLUSH);
		if (rc < 0)
			pr_err("%s[%p]: flush cmd failed rc=%d\n",
				__func__, audio, rc);
		/* Not in stop state, reenable the stream */
		if (audio->stopped == 0) {
			rc = audio_aio_enable(audio);
			if (rc)
				pr_err("%s[%p]:audio re-enable failed\n",
					__func__, audio);
			else {
				audio->enabled = 1;
				if (audio->drv_status & ADRV_STATUS_PAUSE)
					audio->drv_status &= ~ADRV_STATUS_PAUSE;
			}
		}
	}
	pr_debug("%s[%p]:in_bytes %d\n",
			__func__, audio, atomic_read(&audio->in_bytes));
	pr_debug("%s[%p]:in_samples %d\n",
			__func__, audio, atomic_read(&audio->in_samples));
	atomic_set(&audio->in_bytes, 0);
	atomic_set(&audio->in_samples, 0);
	return 0;
}

static int audio_aio_outport_flush(struct q6audio_aio *audio)
{
	int rc;

	rc = q6asm_cmd(audio->ac, CMD_OUT_FLUSH);
	if (rc < 0)
		pr_err("%s[%p}: output port flush cmd failed rc=%d\n",
			__func__, audio, rc);
	return rc;
}

/* Write buffer to DSP / Handle Ack from DSP */
void audio_aio_async_write_ack(struct q6audio_aio *audio, uint32_t token,
				uint32_t *payload)
{
	unsigned long flags;
	union msm_audio_event_payload event_payload;
	struct audio_aio_buffer_node *used_buf;

	/* No active flush in progress */
	if (audio->wflush)
		return;

	spin_lock_irqsave(&audio->dsp_lock, flags);
	if (list_empty(&audio->out_queue)) {
		pr_warning("%s: ingore unexpected event from dsp\n", __func__);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		return;
	}
	used_buf = list_first_entry(&audio->out_queue,
					struct audio_aio_buffer_node, list);
	if (token == used_buf->token) {
		list_del(&used_buf->list);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		pr_debug("%s[%p]:consumed buffer\n", __func__, audio);
		event_payload.aio_buf = used_buf->buf;
		audio_aio_post_event(audio, AUDIO_EVENT_WRITE_DONE,
					event_payload);
		kfree(used_buf);
		if (list_empty(&audio->out_queue) &&
			(audio->drv_status & ADRV_STATUS_FSYNC)) {
			pr_debug("%s[%p]: list is empty, reached EOS in Tunnel\n",
				 __func__, audio);
			wake_up(&audio->write_wait);
		}
	} else {
		pr_err("%s[%p]:expected=%x ret=%x\n",
			__func__, audio, used_buf->token, token);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
	}
}

/* ------------------- device --------------------- */
void audio_aio_async_out_flush(struct q6audio_aio *audio)
{
	struct audio_aio_buffer_node *buf_node;
	struct list_head *ptr, *next;
	union msm_audio_event_payload payload;
	unsigned long flags;

	pr_debug("%s[%p}\n", __func__, audio);
	/* EOS followed by flush, EOS response not guranteed, free EOS i/p
	buffer */
	spin_lock_irqsave(&audio->dsp_lock, flags);

	if (audio->eos_flag && (audio->eos_write_payload.aio_buf.buf_addr)) {
		pr_debug("%s[%p]: EOS followed by flush received,acknowledge"\
			" eos i/p buffer immediately\n", __func__, audio);
		audio_aio_post_event(audio, AUDIO_EVENT_WRITE_DONE,
				audio->eos_write_payload);
		memset(&audio->eos_write_payload , 0,
			sizeof(union msm_audio_event_payload));
	}
	spin_unlock_irqrestore(&audio->dsp_lock, flags);
	list_for_each_safe(ptr, next, &audio->out_queue) {
		buf_node = list_entry(ptr, struct audio_aio_buffer_node, list);
		list_del(&buf_node->list);
		payload.aio_buf = buf_node->buf;
		audio_aio_post_event(audio, AUDIO_EVENT_WRITE_DONE, payload);
		kfree(buf_node);
		pr_debug("%s[%p]: Propagate WRITE_DONE during flush\n",
				__func__, audio);
	}
}

void audio_aio_async_in_flush(struct q6audio_aio *audio)
{
	struct audio_aio_buffer_node *buf_node;
	struct list_head *ptr, *next;
	union msm_audio_event_payload payload;

	pr_debug("%s[%p]\n", __func__, audio);
	list_for_each_safe(ptr, next, &audio->in_queue) {
		buf_node = list_entry(ptr, struct audio_aio_buffer_node, list);
		list_del(&buf_node->list);
		/* Forcefull send o/p eos buffer after flush, if no eos response
		 * received by dsp even after sending eos command */
		if ((audio->eos_rsp != 1) && audio->eos_flag) {
			pr_debug("%s[%p]: send eos on o/p buffer during flush\n",
				 __func__, audio);
			payload.aio_buf = buf_node->buf;
			payload.aio_buf.data_len =
					insert_eos_buf(audio, buf_node);
			audio->eos_flag = 0;
		} else {
			payload.aio_buf = buf_node->buf;
			payload.aio_buf.data_len =
					insert_meta_data_flush(audio, buf_node);
		}
		audio_aio_post_event(audio, AUDIO_EVENT_READ_DONE, payload);
		kfree(buf_node);
		pr_debug("%s[%p]: Propagate READ_DONE during flush\n",
				__func__, audio);
	}
}

int audio_aio_enable(struct q6audio_aio  *audio)
{
	/* 2nd arg: 0 -> run immediately
	3rd arg: 0 -> msw_ts, 4th arg: 0 ->lsw_ts */
	return q6asm_run(audio->ac, 0x00, 0x00, 0x00);
}

int audio_aio_disable(struct q6audio_aio *audio)
{
	int rc = 0;
	if (audio->opened) {
		audio->enabled = 0;
		audio->opened = 0;
		pr_debug("%s[%p]: inbytes[%d] insamples[%d]\n", __func__,
			audio, atomic_read(&audio->in_bytes),
			atomic_read(&audio->in_samples));
		/* Close the session */
		rc = q6asm_cmd(audio->ac, CMD_CLOSE);
		if (rc < 0)
			pr_err("%s[%p]:Failed to close the session rc=%d\n",
				__func__, audio, rc);
		audio->stopped = 1;
		wake_up(&audio->write_wait);
		wake_up(&audio->cmd_wait);
	}
	pr_debug("%s[%p]:enabled[%d]\n", __func__, audio, audio->enabled);
	return rc;
}

void audio_aio_reset_ion_region(struct q6audio_aio *audio)
{
	struct audio_aio_ion_region *region;
	struct list_head *ptr, *next;

	list_for_each_safe(ptr, next, &audio->ion_region_queue) {
		region = list_entry(ptr, struct audio_aio_ion_region, list);
		list_del(&region->list);
		msm_audio_ion_free_legacy(audio->client, region->handle);
		kfree(region);
	}

	return;
}

void audio_aio_reset_event_queue(struct q6audio_aio *audio)
{
	unsigned long flags;
	struct audio_aio_event *drv_evt;
	struct list_head *ptr, *next;

	spin_lock_irqsave(&audio->event_queue_lock, flags);
	list_for_each_safe(ptr, next, &audio->event_queue) {
		drv_evt = list_first_entry(&audio->event_queue,
				   struct audio_aio_event, list);
		list_del(&drv_evt->list);
		kfree(drv_evt);
	}
	list_for_each_safe(ptr, next, &audio->free_event_queue) {
		drv_evt = list_first_entry(&audio->free_event_queue,
				   struct audio_aio_event, list);
		list_del(&drv_evt->list);
		kfree(drv_evt);
	}
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);

	return;
}

static void audio_aio_unmap_ion_region(struct q6audio_aio *audio)
{
	struct audio_aio_ion_region *region;
	struct list_head *ptr, *next;
	int rc = -EINVAL;

	pr_debug("%s[%p]:\n", __func__, audio);
	list_for_each_safe(ptr, next, &audio->ion_region_queue) {
		region = list_entry(ptr, struct audio_aio_ion_region, list);
		if (region != NULL) {
			pr_debug("%s[%p]: phy_address = 0x%pa\n",
				__func__, audio, &region->paddr);
			rc = q6asm_memory_unmap(audio->ac,
						region->paddr, IN);
			if (rc < 0)
				pr_err("%s[%p]: memory unmap failed\n",
					__func__, audio);
		}
	}
}

#ifdef CONFIG_USE_DEV_CTRL_VOLUME

static void audio_aio_listner(u32 evt_id, union auddev_evt_data *evt_payload,
			void *private_data)
{
	struct q6audio_aio *audio = (struct q6audio_aio *) private_data;
	int rc  = 0;

	switch (evt_id) {
	case AUDDEV_EVT_STREAM_VOL_CHG:
		audio->volume = evt_payload->session_vol;
		pr_debug("%s[%p]: AUDDEV_EVT_STREAM_VOL_CHG, stream vol %d, enabled = %d\n",
			__func__, audio, audio->volume, audio->enabled);
		if (audio->enabled == 1) {
			if (audio->ac) {
				rc = q6asm_set_volume(audio->ac, audio->volume);
				if (rc < 0) {
					pr_err("%s[%p]: Send Volume command failed rc=%d\n",
						__func__, audio, rc);
				}
			}
		}
		break;
	default:
		pr_err("%s[%p]:ERROR:wrong event\n", __func__, audio);
		break;
	}
}

int register_volume_listener(struct q6audio_aio *audio)
{
	int rc  = 0;
	audio->device_events = AUDDEV_EVT_STREAM_VOL_CHG;
	audio->drv_status &= ~ADRV_STATUS_PAUSE;

	rc = auddev_register_evt_listner(audio->device_events,
					AUDDEV_CLNT_DEC,
					audio->ac->session,
					audio_aio_listner,
					(void *)audio);
	if (rc < 0) {
		pr_err("%s[%p]: Event listener failed\n", __func__, audio);
		rc = -EACCES;
	}
	return rc;
}
void unregister_volume_listener(struct q6audio_aio *audio)
{
	auddev_unregister_evt_listner(AUDDEV_CLNT_DEC, audio->ac->session);
}

int enable_volume_ramp(struct q6audio_aio *audio)
{
	int rc = 0;
	struct asm_softpause_params softpause;
	struct asm_softvolume_params softvol;

	if (audio->ac == NULL)
		return -EINVAL;
	pr_debug("%s[%p]\n", __func__, audio);
	softpause.enable = SOFT_PAUSE_ENABLE;
	softpause.period = SOFT_PAUSE_PERIOD;
	softpause.step = SOFT_PAUSE_STEP;
	softpause.rampingcurve = SOFT_PAUSE_CURVE_LINEAR;

	softvol.period = SOFT_VOLUME_PERIOD;
	softvol.step = SOFT_VOLUME_STEP;
	softvol.rampingcurve = SOFT_VOLUME_CURVE_LINEAR;

	if (softpause.rampingcurve == SOFT_PAUSE_CURVE_LINEAR)
		softpause.step = SOFT_PAUSE_STEP_LINEAR;
	if (softvol.rampingcurve == SOFT_VOLUME_CURVE_LINEAR)
		softvol.step = SOFT_VOLUME_STEP_LINEAR;
	rc = q6asm_set_volume(audio->ac, audio->volume);
	if (rc < 0) {
		pr_err("%s: Send Volume command failed rc=%d\n",
			__func__, rc);
		return rc;
	}
	rc = q6asm_set_softpause(audio->ac, &softpause);
	if (rc < 0) {
		pr_err("%s: Send SoftPause Param failed rc=%d\n",
			__func__, rc);
		return rc;
	}
	rc = q6asm_set_softvolume(audio->ac, &softvol);
	if (rc < 0) {
		pr_err("%s: Send SoftVolume Param failed rc=%d\n",
		__func__, rc);
		return rc;
	}
	/* disable mute by default */
	rc = q6asm_set_mute(audio->ac, 0);
	if (rc < 0) {
		pr_err("%s: Send mute command failed rc=%d\n",
			__func__, rc);
		return rc;
	}
	return rc;
}

#else /*CONFIG_USE_DEV_CTRL_VOLUME*/
int register_volume_listener(struct q6audio_aio *audio)
{
	return 0;/* do nothing */
}
void unregister_volume_listener(struct q6audio_aio *audio)
{
	return;/* do nothing */
}
int enable_volume_ramp(struct q6audio_aio *audio)
{
	return 0; /* do nothing */
}
#endif /*CONFIG_USE_DEV_CTRL_VOLUME*/

int audio_aio_release(struct inode *inode, struct file *file)
{
	struct q6audio_aio *audio = file->private_data;
	pr_debug("%s[%p]\n", __func__, audio);
	mutex_lock(&audio->lock);
	audio->wflush = 1;
	if (audio->wakelock_voted &&
		(audio->audio_ws_mgr != NULL) &&
		(audio->miscdevice != NULL)) {
		audio->wakelock_voted = false;
		mutex_lock(&audio->audio_ws_mgr->ws_lock);
		if ((audio->audio_ws_mgr->ref_cnt > 0) &&
				(--audio->audio_ws_mgr->ref_cnt == 0)) {
			pm_relax(audio->miscdevice->this_device);
		}
		mutex_unlock(&audio->audio_ws_mgr->ws_lock);
	}
	if (audio->enabled)
		audio_aio_flush(audio);
	audio->wflush = 0;
	audio->drv_ops.out_flush(audio);
	audio->drv_ops.in_flush(audio);
	audio_aio_disable(audio);
	audio_aio_unmap_ion_region(audio);
	audio_aio_reset_ion_region(audio);
	msm_audio_ion_client_destroy(audio->client);
	audio->event_abort = 1;
	wake_up(&audio->event_wait);
	audio_aio_reset_event_queue(audio);
	q6asm_audio_client_free(audio->ac);
	mutex_unlock(&audio->lock);
	mutex_destroy(&audio->lock);
	mutex_destroy(&audio->read_lock);
	mutex_destroy(&audio->write_lock);
	mutex_destroy(&audio->get_event_lock);
	unregister_volume_listener(audio);

#ifdef CONFIG_DEBUG_FS
	if (audio->dentry)
		debugfs_remove(audio->dentry);
#endif
	kfree(audio->codec_cfg);
	kfree(audio);
	return 0;
}

int audio_aio_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
	int rc = 0;
	struct q6audio_aio *audio = file->private_data;

	if (!audio->enabled || audio->feedback)
		return -EINVAL;

	/* Blocking client sends more data */
	mutex_lock(&audio->lock);
	audio->drv_status |= ADRV_STATUS_FSYNC;
	mutex_unlock(&audio->lock);

	pr_debug("%s[%p]:\n", __func__, audio);

	audio->eos_rsp = 0;

	pr_debug("%s[%p]Wait for write done from DSP\n", __func__, audio);
	rc = wait_event_interruptible(audio->write_wait,
					(list_empty(&audio->out_queue)) ||
					audio->wflush || audio->stopped);

	if (audio->stopped || audio->wflush) {
		pr_debug("%s[%p]: Audio Flushed or Stopped,this is not EOS\n"
			, __func__, audio);
		audio->wflush = 0;
		rc = -EBUSY;
	}

	if (rc < 0) {
		pr_err("%s[%p]: wait event for list_empty failed, rc = %d\n",
			__func__, audio, rc);
		goto done;
	}

	rc = q6asm_cmd(audio->ac, CMD_EOS);
	pr_debug("%s[%p]: EOS cmd sent to DSP\n", __func__, audio);

	if (rc < 0)
		pr_err("%s[%p]: q6asm_cmd failed, rc = %d",
			__func__, audio, rc);

	pr_debug("%s[%p]: wait for RENDERED_EOS from DSP\n"
		, __func__, audio);
	rc = wait_event_interruptible(audio->write_wait,
					(audio->eos_rsp || audio->wflush ||
					audio->stopped));

	if (rc < 0) {
		pr_err("%s[%p]: wait event for eos_rsp failed, rc = %d\n",
			__func__, audio, rc);
		goto done;
	}

	if (audio->stopped || audio->wflush) {
		audio->wflush = 0;
		pr_debug("%s[%p]: Audio Flushed or Stopped,this is not EOS\n"
			, __func__, audio);
		rc = -EBUSY;
	}

	if (audio->eos_rsp == 1)
		pr_debug("%s[%p]: EOS\n", __func__, audio);


done:
	mutex_lock(&audio->lock);
	audio->drv_status &= ~ADRV_STATUS_FSYNC;
	mutex_unlock(&audio->lock);

	return rc;
}

static int audio_aio_events_pending(struct q6audio_aio *audio)
{
	unsigned long flags;
	int empty;

	spin_lock_irqsave(&audio->event_queue_lock, flags);
	empty = !list_empty(&audio->event_queue);
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);
	return empty || audio->event_abort;
}

static long audio_aio_process_event_req_common(struct q6audio_aio *audio,
					struct msm_audio_event *usr_evt)
{
	long rc;
	struct audio_aio_event *drv_evt = NULL;
	int timeout;
	unsigned long flags;

	timeout = usr_evt->timeout_ms;

	if (timeout > 0) {
		rc = wait_event_interruptible_timeout(audio->event_wait,
						audio_aio_events_pending
						(audio),
						msecs_to_jiffies
						(timeout));
		if (rc == 0)
			return -ETIMEDOUT;
	} else {
		rc = wait_event_interruptible(audio->event_wait,
		audio_aio_events_pending(audio));
	}
	if (rc < 0)
		return rc;

	if (audio->event_abort) {
		audio->event_abort = 0;
		return -ENODEV;
	}

	rc = 0;

	spin_lock_irqsave(&audio->event_queue_lock, flags);
	if (!list_empty(&audio->event_queue)) {
		drv_evt = list_first_entry(&audio->event_queue,
		   struct audio_aio_event, list);
		list_del(&drv_evt->list);
	}
	if (drv_evt) {
		usr_evt->event_type = drv_evt->event_type;
		usr_evt->event_payload = drv_evt->payload;
		list_add_tail(&drv_evt->list, &audio->free_event_queue);
	} else {
		pr_err("%s[%p]:Unexpected path\n", __func__, audio);
		spin_unlock_irqrestore(&audio->event_queue_lock, flags);
		return -EPERM;
	}
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);

	if (drv_evt->event_type == AUDIO_EVENT_WRITE_DONE) {
		pr_debug("%s[%p]:posted AUDIO_EVENT_WRITE_DONE to user\n",
			__func__, audio);
		mutex_lock(&audio->write_lock);
		audio_aio_ion_fixup(audio, drv_evt->payload.aio_buf.buf_addr,
		drv_evt->payload.aio_buf.buf_len, 0, 0);
		mutex_unlock(&audio->write_lock);
	} else if (drv_evt->event_type == AUDIO_EVENT_READ_DONE) {
		pr_debug("%s[%p]:posted AUDIO_EVENT_READ_DONE to user\n",
			__func__, audio);
		mutex_lock(&audio->read_lock);
		audio_aio_ion_fixup(audio, drv_evt->payload.aio_buf.buf_addr,
		drv_evt->payload.aio_buf.buf_len, 0, 0);
		mutex_unlock(&audio->read_lock);
	}

	/* Some read buffer might be held up in DSP,release all
	 * Once EOS indicated
	 */
	if (audio->eos_rsp && !list_empty(&audio->in_queue)) {
		pr_debug("%s[%p]:Send flush command to release read buffers"\
			" held up in DSP\n", __func__, audio);
		mutex_lock(&audio->lock);
		audio_aio_flush(audio);
		mutex_unlock(&audio->lock);
	}

	return rc;
}

static long audio_aio_process_event_req(struct q6audio_aio *audio,
					void __user *arg)
{
	long rc;
	struct msm_audio_event usr_evt;

	if (copy_from_user(&usr_evt, arg, sizeof(struct msm_audio_event))) {
		pr_err("%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	rc = audio_aio_process_event_req_common(audio, &usr_evt);

	if (copy_to_user(arg, &usr_evt, sizeof(usr_evt))) {
		pr_err("%s: copy_to_user failed\n", __func__);
		rc = -EFAULT;
	}
	return rc;
}

#ifdef CONFIG_COMPAT

struct msm_audio_aio_buf32 {
	compat_uptr_t buf_addr;
	u32 buf_len;
	u32 data_len;
	compat_uptr_t private_data;
	u16 mfield_sz; /*only useful for data has meta field */
};

struct msm_audio_bitstream_info32 {
	u32 codec_type;
	u32 chan_info;
	u32 sample_rate;
	u32 bit_stream_info;
	u32 bit_rate;
	u32 unused[3];
};

struct msm_audio_bitstream_error_info32 {
	u32 dec_id;
	u32 err_msg_indicator;
	u32 err_type;
};

union msm_audio_event_payload32 {
	struct msm_audio_aio_buf32 aio_buf;
	struct msm_audio_bitstream_info32 stream_info;
	struct msm_audio_bitstream_error_info32 error_info;
	s32 reserved;
};

struct msm_audio_event32 {
	s32 event_type;
	s32 timeout_ms;
	union msm_audio_event_payload32 event_payload;
};

static long audio_aio_process_event_req_compat(struct q6audio_aio *audio,
					void __user *arg)
{
	long rc;
	struct msm_audio_event32 usr_evt_32;
	struct msm_audio_event usr_evt;

	if (copy_from_user(&usr_evt_32, arg,
				sizeof(struct msm_audio_event32))) {
		pr_err("%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}
	usr_evt.timeout_ms = usr_evt_32.timeout_ms;

	rc = audio_aio_process_event_req_common(audio, &usr_evt);

	usr_evt_32.event_type = usr_evt.event_type;
	switch (usr_evt_32.event_type) {
	case AUDIO_EVENT_SUSPEND:
	case AUDIO_EVENT_RESUME:
	case AUDIO_EVENT_WRITE_DONE:
	case AUDIO_EVENT_READ_DONE:
		usr_evt_32.event_payload.aio_buf.buf_addr =
			ptr_to_compat(usr_evt.event_payload.aio_buf.buf_addr);
		usr_evt_32.event_payload.aio_buf.buf_len =
			usr_evt.event_payload.aio_buf.buf_len;
		usr_evt_32.event_payload.aio_buf.data_len =
			usr_evt.event_payload.aio_buf.data_len;
		usr_evt_32.event_payload.aio_buf.private_data =
		ptr_to_compat(usr_evt.event_payload.aio_buf.private_data);
		usr_evt_32.event_payload.aio_buf.mfield_sz =
			usr_evt.event_payload.aio_buf.mfield_sz;
		break;
	case AUDIO_EVENT_STREAM_INFO:
		usr_evt_32.event_payload.stream_info.codec_type =
			usr_evt.event_payload.stream_info.codec_type;
		usr_evt_32.event_payload.stream_info.chan_info =
			usr_evt.event_payload.stream_info.chan_info;
		usr_evt_32.event_payload.stream_info.sample_rate =
			usr_evt.event_payload.stream_info.sample_rate;
		usr_evt_32.event_payload.stream_info.bit_stream_info =
			usr_evt.event_payload.stream_info.bit_stream_info;
		usr_evt_32.event_payload.stream_info.bit_rate =
			usr_evt.event_payload.stream_info.bit_rate;
		break;
	case AUDIO_EVENT_BITSTREAM_ERROR_INFO:
		usr_evt_32.event_payload.error_info.dec_id =
			usr_evt.event_payload.error_info.dec_id;
		usr_evt_32.event_payload.error_info.err_msg_indicator =
			usr_evt.event_payload.error_info.err_msg_indicator;
		usr_evt_32.event_payload.error_info.err_type =
			usr_evt.event_payload.error_info.err_type;
		break;
	default:
		pr_debug("%s: unknown audio event type = %d rc = %ld",
			 __func__, usr_evt_32.event_type, rc);
		return rc;
	}
	if (copy_to_user(arg, &usr_evt_32, sizeof(usr_evt_32))) {
		pr_err("%s: copy_to_user failed\n", __func__);
		rc = -EFAULT;
	}
	return rc;
}
#endif

static int audio_aio_ion_check(struct q6audio_aio *audio,
				void *vaddr, unsigned long len)
{
	struct audio_aio_ion_region *region_elt;
	struct audio_aio_ion_region t = {.vaddr = vaddr, .len = len };

	list_for_each_entry(region_elt, &audio->ion_region_queue, list) {
		if (CONTAINS(region_elt, &t) || CONTAINS(&t, region_elt) ||
			OVERLAPS(region_elt, &t)) {
			pr_err("%s[%p]:region (vaddr %p len %ld) clashes with registered region (vaddr %p paddr %pa len %ld)\n",
				__func__, audio, vaddr, len,
				region_elt->vaddr,
				&region_elt->paddr, region_elt->len);
			return -EINVAL;
		}
	}

	return 0;
}

static int audio_aio_ion_add(struct q6audio_aio *audio,
				struct msm_audio_ion_info *info)
{
	ion_phys_addr_t paddr = 0;
	size_t len = 0;
	struct audio_aio_ion_region *region;
	int rc = -EINVAL;
	struct ion_handle *handle = NULL;
	unsigned long ionflag;
	void *kvaddr = NULL;

	pr_debug("%s[%p]:\n", __func__, audio);
	region = kmalloc(sizeof(*region), GFP_KERNEL);

	if (!region) {
		rc = -ENOMEM;
		goto end;
	}

	rc = msm_audio_ion_import_legacy("Audio_Dec_Client", audio->client,
				&handle, info->fd, &ionflag,
				0, &paddr, &len, &kvaddr);
	if (rc) {
		pr_err("%s: msm audio ion alloc failed\n", __func__);
		goto import_error;
	}

	rc = audio_aio_ion_check(audio, info->vaddr, len);
	if (rc < 0) {
		pr_err("%s: audio_aio_ion_check failed\n", __func__);
		goto ion_error;
	}

	region->handle = handle;
	region->vaddr = info->vaddr;
	region->fd = info->fd;
	region->paddr = paddr;
	region->kvaddr = kvaddr;
	region->len = len;
	region->ref_cnt = 0;
	pr_debug("%s[%p]:add region paddr %pa vaddr %p, len %lu kvaddr %p\n",
		__func__, audio,
		&region->paddr, region->vaddr, region->len,
		region->kvaddr);
	list_add_tail(&region->list, &audio->ion_region_queue);
	rc = q6asm_memory_map(audio->ac,  paddr, IN, len, 1);
	if (rc < 0) {
		pr_err("%s[%p]: memory map failed\n", __func__, audio);
		goto mmap_error;
	} else {
		goto end;
	}
mmap_error:
	list_del(&region->list);
ion_error:
	msm_audio_ion_free_legacy(audio->client, handle);
import_error:
	kfree(region);
end:
	return rc;
}

static int audio_aio_ion_remove(struct q6audio_aio *audio,
				struct msm_audio_ion_info *info)
{
	struct audio_aio_ion_region *region;
	struct list_head *ptr, *next;
	int rc = -EINVAL;

	pr_debug("%s[%p]:info fd %d vaddr %p\n",
		__func__, audio, info->fd, info->vaddr);

	list_for_each_safe(ptr, next, &audio->ion_region_queue) {
		region = list_entry(ptr, struct audio_aio_ion_region, list);

		if ((region->fd == info->fd) &&
			(region->vaddr == info->vaddr)) {
			if (region->ref_cnt) {
				pr_debug("%s[%p]:region %p in use ref_cnt %d\n",
					__func__, audio, region,
					region->ref_cnt);
				break;
			}
			pr_debug("%s[%p]:remove region fd %d vaddr %p\n",
				__func__, audio, info->fd, info->vaddr);
			rc = q6asm_memory_unmap(audio->ac,
						region->paddr, IN);
			if (rc < 0)
				pr_err("%s[%p]: memory unmap failed\n",
					__func__, audio);

			list_del(&region->list);
			msm_audio_ion_free_legacy(audio->client,
						 region->handle);
			kfree(region);
			rc = 0;
			break;
		}
	}

	return rc;
}

static int audio_aio_async_write(struct q6audio_aio *audio,
				struct audio_aio_buffer_node *buf_node)
{
	int rc;
	struct audio_client *ac;
	struct audio_aio_write_param param;

	if (!audio || !buf_node) {
		pr_err("%s NULL pointer audio=[0x%p], buf_node=[0x%p]\n",
			__func__, audio, buf_node);
		return -EINVAL;
	}
	pr_debug("%s[%p]: Send write buff %p phy %pa len %d meta_enable = %d\n",
		__func__, audio, buf_node, &buf_node->paddr,
		buf_node->buf.data_len,
		audio->buf_cfg.meta_info_enable);
	pr_debug("%s[%p]: flags = 0x%x\n", __func__, audio,
		buf_node->meta_info.meta_in.nflags);

	ac = audio->ac;
	/* Offset with  appropriate meta */
	if (audio->feedback) {
		/* Non Tunnel mode */
		param.paddr = buf_node->paddr + sizeof(struct dec_meta_in);
		param.len = buf_node->buf.data_len - sizeof(struct dec_meta_in);
	} else {
		/* Tunnel mode */
		param.paddr = buf_node->paddr;
		param.len = buf_node->buf.data_len;
	}
	param.msw_ts = buf_node->meta_info.meta_in.ntimestamp.highpart;
	param.lsw_ts = buf_node->meta_info.meta_in.ntimestamp.lowpart;
	param.flags  = buf_node->meta_info.meta_in.nflags;
	/* If no meta_info enaled, indicate no time stamp valid */
	if (!audio->buf_cfg.meta_info_enable)
		param.flags = 0xFF00;

	if (buf_node->meta_info.meta_in.nflags & AUDIO_DEC_EOF_SET)
		param.flags |= AUDIO_DEC_EOF_SET;

	param.uid = ac->session;
	/* Read command will populate paddr as token */
	buf_node->token = ac->session;
	rc = q6asm_async_write(ac, &param);
	if (rc < 0)
		pr_err("%s[%p]:failed\n", __func__, audio);
	return rc;
}

void audio_aio_post_event(struct q6audio_aio *audio, int type,
			union msm_audio_event_payload payload)
{
	struct audio_aio_event *e_node = NULL;
	unsigned long flags;

	spin_lock_irqsave(&audio->event_queue_lock, flags);

	if (!list_empty(&audio->free_event_queue)) {
		e_node = list_first_entry(&audio->free_event_queue,
					struct audio_aio_event, list);
		list_del(&e_node->list);
	} else {
		e_node = kmalloc(sizeof(struct audio_aio_event), GFP_ATOMIC);
		if (!e_node) {
			pr_err("%s[%p]:No mem to post event %d\n",
				__func__, audio, type);
			spin_unlock_irqrestore(&audio->event_queue_lock, flags);
			return;
		}
	}

	e_node->event_type = type;
	e_node->payload = payload;

	list_add_tail(&e_node->list, &audio->event_queue);
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);
	wake_up(&audio->event_wait);
}

static int audio_aio_async_read(struct q6audio_aio *audio,
				struct audio_aio_buffer_node *buf_node)
{
	struct audio_client *ac;
	struct audio_aio_read_param param;
	int rc;

	pr_debug("%s[%p]: Send read buff %p phy %pa len %d\n",
		__func__, audio, buf_node,
		&buf_node->paddr, buf_node->buf.buf_len);
	ac = audio->ac;
	/* Provide address so driver can append nr frames information */
	param.paddr = buf_node->paddr +
		sizeof(struct dec_meta_out);
	param.len = buf_node->buf.buf_len -
		sizeof(struct dec_meta_out);
	param.uid = param.paddr;
	/* Write command will populate paddr as token */
	buf_node->token = param.paddr;
	rc = q6asm_async_read(ac, &param);
	if (rc < 0)
		pr_err("%s[%p]:failed\n", __func__, audio);
	return rc;
}

static int audio_aio_buf_add_shared(struct q6audio_aio *audio, u32 dir,
				struct audio_aio_buffer_node *buf_node)
{
	unsigned long flags;
	int ret = 0;
	pr_debug("%s[%p]:node %p dir %x buf_addr %p buf_len %d data_len %d\n",
		 __func__, audio, buf_node, dir, buf_node->buf.buf_addr,
		buf_node->buf.buf_len, buf_node->buf.data_len);
	buf_node->paddr = audio_aio_ion_fixup(audio, buf_node->buf.buf_addr,
						buf_node->buf.buf_len, 1,
						&buf_node->kvaddr);
	if (dir) {
		/* write */
		if (!buf_node->paddr ||
			(buf_node->paddr & 0x1) ||
			(!audio->feedback && !buf_node->buf.data_len)) {
			kfree(buf_node);
			return -EINVAL;
		}
		extract_meta_out_info(audio, buf_node, 1);
		/* Not a EOS buffer */
		if (!(buf_node->meta_info.meta_in.nflags & AUDIO_DEC_EOS_SET)) {
			spin_lock_irqsave(&audio->dsp_lock, flags);
			ret = audio_aio_async_write(audio, buf_node);
			/* EOS buffer handled in driver */
			list_add_tail(&buf_node->list, &audio->out_queue);
			spin_unlock_irqrestore(&audio->dsp_lock, flags);
		} else if (buf_node->meta_info.meta_in.nflags
				   & AUDIO_DEC_EOS_SET) {
			if (!audio->wflush) {
				pr_debug("%s[%p]:Send EOS cmd at i/p\n",
					__func__, audio);
				/* Driver will forcefully post writedone event
				 * once eos ack recived from DSP
				 */
				audio->eos_write_payload.aio_buf =\
						buf_node->buf;
				audio->eos_flag = 1;
				audio->eos_rsp = 0;
				q6asm_cmd(audio->ac, CMD_EOS);
				kfree(buf_node);
			} else { /* Flush in progress, send back i/p
				  * EOS buffer as is
				  */
				union msm_audio_event_payload event_payload;
				event_payload.aio_buf = buf_node->buf;
				audio_aio_post_event(audio,
						AUDIO_EVENT_WRITE_DONE,
						event_payload);
				kfree(buf_node);
			}
		}
	} else {
		/* read */
		if (!buf_node->paddr ||
			(buf_node->paddr & 0x1) ||
			(buf_node->buf.buf_len < PCM_BUFSZ_MIN)) {
			kfree(buf_node);
			return -EINVAL;
		}
		/* No EOS reached */
		if (!audio->eos_rsp) {
			spin_lock_irqsave(&audio->dsp_lock, flags);
			ret = audio_aio_async_read(audio, buf_node);
			/* EOS buffer handled in driver */
			list_add_tail(&buf_node->list, &audio->in_queue);
			spin_unlock_irqrestore(&audio->dsp_lock, flags);
		}
		/* EOS reached at input side fake all upcoming read buffer to
		 * indicate the same
		 */
		else {
			union msm_audio_event_payload event_payload;
			event_payload.aio_buf = buf_node->buf;
			event_payload.aio_buf.data_len =
				insert_eos_buf(audio, buf_node);
			pr_debug("%s[%p]: propagate READ_DONE as EOS done\n",\
				__func__, audio);
			audio_aio_post_event(audio, AUDIO_EVENT_READ_DONE,
					event_payload);
			kfree(buf_node);
		}
	}
	return ret;
}
#ifdef CONFIG_COMPAT
static int audio_aio_buf_add_compat(struct q6audio_aio *audio, u32 dir,
				void __user *arg)
{
	struct audio_aio_buffer_node *buf_node;
	struct msm_audio_aio_buf32 aio_buf_32;

	buf_node = kzalloc(sizeof(*buf_node), GFP_KERNEL);

	if (!buf_node) {
		pr_err("%s: Buffer node alloc failed\n", __func__);
		return -ENOMEM;
	}


	if (copy_from_user(&aio_buf_32, arg, sizeof(aio_buf_32))) {
		kfree(buf_node);
		pr_err("%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	buf_node->buf.buf_addr = compat_ptr(aio_buf_32.buf_addr);
	buf_node->buf.buf_len = aio_buf_32.buf_len;
	buf_node->buf.data_len = aio_buf_32.data_len;
	buf_node->buf.private_data = compat_ptr(aio_buf_32.private_data);
	buf_node->buf.mfield_sz = aio_buf_32.mfield_sz;

	return audio_aio_buf_add_shared(audio, dir, buf_node);
}
#endif

static int audio_aio_buf_add(struct q6audio_aio *audio, u32 dir,
				void __user *arg)
{
	struct audio_aio_buffer_node *buf_node;

	buf_node = kzalloc(sizeof(*buf_node), GFP_KERNEL);

	if (!buf_node) {
		pr_err("%s: Buffer node alloc failed\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(&buf_node->buf, arg, sizeof(buf_node->buf))) {
		kfree(buf_node);
		pr_err("%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	return audio_aio_buf_add_shared(audio, dir, buf_node);
}

void audio_aio_ioport_reset(struct q6audio_aio *audio)
{
	if (audio->drv_status & ADRV_STATUS_AIO_INTF) {
		/* If fsync is in progress, make sure
		 * return value of fsync indicates
		 * abort due to flush
		 */
		if (audio->drv_status & ADRV_STATUS_FSYNC) {
			pr_debug("%s[%p]:fsync in progress\n", __func__, audio);
			audio->drv_ops.out_flush(audio);
		} else
			audio->drv_ops.out_flush(audio);
		if (audio->feedback == NON_TUNNEL_MODE)
			audio->drv_ops.in_flush(audio);
	}
}

int audio_aio_open(struct q6audio_aio *audio, struct file *file)
{
	int rc = 0;
	int i;
	struct audio_aio_event *e_node = NULL;
	struct list_head *ptr, *next;

	/* Settings will be re-config at AUDIO_SET_CONFIG,
	 * but at least we need to have initial config
	 */
	audio->str_cfg.buffer_size = FRAME_SIZE;
	audio->str_cfg.buffer_count = FRAME_NUM;
	audio->pcm_cfg.buffer_count = PCM_BUF_COUNT;
	audio->pcm_cfg.sample_rate = 48000;
	audio->pcm_cfg.channel_count = 2;

	/* Only AIO interface */
	if (file->f_flags & O_NONBLOCK) {
		pr_debug("%s[%p]:set to aio interface\n", __func__, audio);
		audio->drv_status |= ADRV_STATUS_AIO_INTF;
		audio->drv_ops.out_flush = audio_aio_async_out_flush;
		audio->drv_ops.in_flush = audio_aio_async_in_flush;
		q6asm_set_io_mode(audio->ac, ASYNC_IO_MODE);
	} else {
		pr_err("%s[%p]:SIO interface not supported\n",
			__func__, audio);
		rc = -EACCES;
		goto fail;
	}

	/* Initialize all locks of audio instance */
	mutex_init(&audio->lock);
	mutex_init(&audio->read_lock);
	mutex_init(&audio->write_lock);
	mutex_init(&audio->get_event_lock);
	spin_lock_init(&audio->dsp_lock);
	spin_lock_init(&audio->event_queue_lock);
	init_waitqueue_head(&audio->cmd_wait);
	init_waitqueue_head(&audio->write_wait);
	init_waitqueue_head(&audio->event_wait);
	INIT_LIST_HEAD(&audio->out_queue);
	INIT_LIST_HEAD(&audio->in_queue);
	INIT_LIST_HEAD(&audio->ion_region_queue);
	INIT_LIST_HEAD(&audio->free_event_queue);
	INIT_LIST_HEAD(&audio->event_queue);

	audio->drv_ops.out_flush(audio);
	audio->opened = 1;
	file->private_data = audio;
	audio->codec_ioctl = audio_aio_ioctl;
	audio->codec_compat_ioctl = audio_aio_compat_ioctl;
	for (i = 0; i < AUDIO_EVENT_NUM; i++) {
		e_node = kmalloc(sizeof(struct audio_aio_event), GFP_KERNEL);
		if (e_node)
			list_add_tail(&e_node->list, &audio->free_event_queue);
		else {
			pr_err("%s[%p]:event pkt alloc failed\n",
				__func__, audio);
			rc = -ENOMEM;
			goto cleanup;
		}
	}
	audio->client = msm_audio_ion_client_create("Audio_Dec_Client");
	if (IS_ERR_OR_NULL(audio->client)) {
		pr_err("Unable to create ION client\n");
		rc = -ENOMEM;
		goto cleanup;
	}
	pr_debug("Ion client create in audio_aio_open %p", audio->client);

	rc = register_volume_listener(audio);
	if (rc < 0)
		goto ion_cleanup;

	return 0;
ion_cleanup:
	msm_audio_ion_client_destroy(audio->client);
	audio->client = NULL;
cleanup:
	list_for_each_safe(ptr, next, &audio->free_event_queue) {
		e_node = list_first_entry(&audio->free_event_queue,
				   struct audio_aio_event, list);
		list_del(&e_node->list);
		kfree(e_node);
	}
fail:
	return rc;
}

static long audio_aio_shared_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	struct q6audio_aio *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_ABORT_GET_EVENT: {
		audio->event_abort = 1;
		wake_up(&audio->event_wait);
		break;
	}
	case AUDIO_OUTPORT_FLUSH: {
		pr_debug("%s[%p]:AUDIO_OUTPORT_FLUSH\n", __func__, audio);
		mutex_lock(&audio->read_lock);
		rc = audio_aio_outport_flush(audio);
		if (rc < 0) {
			pr_err("%s[%p]: AUDIO_OUTPORT_FLUSH failed\n",
				__func__, audio);
			rc = -EINTR;
		}
		mutex_unlock(&audio->read_lock);
		break;
	}
	case AUDIO_STOP: {
		pr_debug("%s[%p]: AUDIO_STOP session_id[%d]\n", __func__,
				audio, audio->ac->session);
		mutex_lock(&audio->lock);
		audio->stopped = 1;
		rc = audio_aio_flush(audio);
		if (rc < 0) {
			pr_err("%s[%p]:Audio Stop procedure failed rc=%d\n",
				__func__, audio, rc);
			mutex_unlock(&audio->lock);
			break;
		}
		audio->enabled = 0;
		audio->drv_status &= ~ADRV_STATUS_PAUSE;
		if (audio->drv_status & ADRV_STATUS_FSYNC) {
			pr_debug("%s[%p] Waking up the audio_aio_fsync\n",
					__func__, audio);
			wake_up(&audio->write_wait);
		}
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_PAUSE: {
		pr_debug("%s[%p]:AUDIO_PAUSE %ld\n", __func__, audio, arg);
		mutex_lock(&audio->lock);
		if (arg == 1) {
			rc = audio_aio_pause(audio);
			if (rc < 0) {
				pr_err("%s[%p]: pause FAILED rc=%d\n",
					__func__, audio, rc);
				mutex_unlock(&audio->lock);
				break;
			}
			audio->drv_status |= ADRV_STATUS_PAUSE;
		} else if (arg == 0) {
			if (audio->drv_status & ADRV_STATUS_PAUSE) {
				rc = audio_aio_enable(audio);
				if (rc)
					pr_err("%s[%p]: audio enable failed\n",
					__func__, audio);
				else {
					audio->drv_status &= ~ADRV_STATUS_PAUSE;
					audio->enabled = 1;
				}
			}
		}
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_FLUSH: {
		pr_debug("%s[%p]: AUDIO_FLUSH sessionid[%d]\n", __func__,
			audio, audio->ac->session);
		mutex_lock(&audio->lock);
		audio->rflush = 1;
		audio->wflush = 1;
		if (audio->drv_status & ADRV_STATUS_FSYNC) {
			pr_debug("%s[%p] Waking up the audio_aio_fsync\n",
				__func__, audio);
			wake_up(&audio->write_wait);
		}
		/* Flush DSP */
		rc = audio_aio_flush(audio);
		/* Flush input / Output buffer in software*/
		audio_aio_ioport_reset(audio);
		if (rc < 0) {
			pr_err("%s[%p]:AUDIO_FLUSH interrupted\n",
				__func__, audio);
			rc = -EINTR;
		} else {
			audio->rflush = 0;
			if (audio->drv_status & ADRV_STATUS_FSYNC)
				wake_up(&audio->write_wait);
			else
				audio->wflush = 0;

		}
		audio->eos_flag = 0;
		audio->eos_rsp = 0;
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_GET_SESSION_ID: {
		mutex_lock(&audio->lock);
		if (copy_to_user((void *)arg, &audio->ac->session,
			sizeof(u16))) {
			pr_err("%s: copy_to_user for AUDIO_GET_SESSION_ID failed\n",
				__func__);
			rc = -EFAULT;
		}
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_PM_AWAKE: {
		if ((audio->audio_ws_mgr ==  NULL) ||
				(audio->miscdevice == NULL)) {
			pr_err("%s[%p]: invalid ws_mgr or miscdevice",
					__func__, audio);
			rc = -EACCES;
			break;
		}
		pr_debug("%s[%p]:AUDIO_PM_AWAKE\n", __func__, audio);
		mutex_lock(&audio->lock);
		if (!audio->wakelock_voted) {
			audio->wakelock_voted = true;
			mutex_lock(&audio->audio_ws_mgr->ws_lock);
			if (audio->audio_ws_mgr->ref_cnt++ == 0)
				pm_stay_awake(audio->miscdevice->this_device);
			mutex_unlock(&audio->audio_ws_mgr->ws_lock);
		}
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_PM_RELAX: {
		if ((audio->audio_ws_mgr ==  NULL) ||
				(audio->miscdevice == NULL)) {
			pr_err("%s[%p]: invalid ws_mgr or miscdevice",
					__func__, audio);
			rc = -EACCES;
			break;
		}
		pr_debug("%s[%p]:AUDIO_PM_RELAX\n", __func__, audio);
		mutex_lock(&audio->lock);
		if (audio->wakelock_voted) {
			audio->wakelock_voted = false;
			mutex_lock(&audio->audio_ws_mgr->ws_lock);
			if ((audio->audio_ws_mgr->ref_cnt > 0) &&
					(--audio->audio_ws_mgr->ref_cnt == 0)) {
				pm_relax(audio->miscdevice->this_device);
			}
			mutex_unlock(&audio->audio_ws_mgr->ws_lock);
		}
		mutex_unlock(&audio->lock);
		break;
	}
	default:
		pr_err("%s: Unknown ioctl cmd = %d", __func__, cmd);
		rc =  -EINVAL;
	}
	return rc;


}

static long audio_aio_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct q6audio_aio *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_ABORT_GET_EVENT:
	case AUDIO_OUTPORT_FLUSH:
	case AUDIO_STOP:
	case AUDIO_PAUSE:
	case AUDIO_FLUSH:
	case AUDIO_GET_SESSION_ID:
	case AUDIO_PM_AWAKE:
	case AUDIO_PM_RELAX:
		rc = audio_aio_shared_ioctl(file, cmd, arg);
		break;
	case AUDIO_GET_STATS: {
		struct msm_audio_stats stats;
		uint64_t timestamp;
		memset(&stats, 0, sizeof(struct msm_audio_stats));
		stats.byte_count = atomic_read(&audio->in_bytes);
		stats.sample_count = atomic_read(&audio->in_samples);
		rc = q6asm_get_session_time(audio->ac, &timestamp);
		if (rc >= 0)
			memcpy(&stats.unused[0], &timestamp, sizeof(timestamp));
		else
			pr_debug("Error while getting timestamp\n");
		if (copy_to_user((void *)arg, &stats, sizeof(stats))) {
			pr_err("%s: copy_frm_user for AUDIO_GET_STATS failed\n",
				__func__);
			rc = -EFAULT;
		}
		break;
	}
	case AUDIO_GET_EVENT: {
		pr_debug("%s[%p]:AUDIO_GET_EVENT\n", __func__, audio);
		if (mutex_trylock(&audio->get_event_lock)) {
			rc = audio_aio_process_event_req(audio,
						(void __user *)arg);
			mutex_unlock(&audio->get_event_lock);
		} else
			rc = -EBUSY;
		break;
	}
	case AUDIO_ASYNC_WRITE: {
		mutex_lock(&audio->write_lock);
		if (audio->drv_status & ADRV_STATUS_FSYNC)
			rc = -EBUSY;
		else {
			if (audio->enabled)
				rc = audio_aio_buf_add(audio, 1,
						(void __user *)arg);
			else
				rc = -EPERM;
		}
		mutex_unlock(&audio->write_lock);
		break;
	}
	case AUDIO_ASYNC_READ: {
		mutex_lock(&audio->read_lock);
		if (audio->feedback)
			rc = audio_aio_buf_add(audio, 0,
					(void __user *)arg);
		else
			rc = -EPERM;
		mutex_unlock(&audio->read_lock);
		break;
	}

	case AUDIO_GET_STREAM_CONFIG: {
		struct msm_audio_stream_config cfg;
		mutex_lock(&audio->lock);
		memset(&cfg, 0, sizeof(cfg));
		cfg.buffer_size = audio->str_cfg.buffer_size;
		cfg.buffer_count = audio->str_cfg.buffer_count;
		pr_debug("%s[%p]:GET STREAM CFG %d %d\n",
			__func__, audio, cfg.buffer_size, cfg.buffer_count);
		if (copy_to_user((void *)arg, &cfg, sizeof(cfg))) {
			pr_err(
				"%s: copy_to_user for AUDIO_GET_STREAM_CONFIG failed\n",
				__func__);
			rc = -EFAULT;
		}
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_SET_STREAM_CONFIG: {
		struct msm_audio_stream_config cfg;
		pr_debug("%s[%p]:SET STREAM CONFIG\n", __func__, audio);
		mutex_lock(&audio->lock);
		if (copy_from_user(&cfg, (void *)arg, sizeof(cfg))) {
			pr_err(
				"%s: copy_from_user for AUDIO_SET_STREAM_CONFIG failed\n",
				__func__);
			rc = -EFAULT;
			mutex_unlock(&audio->lock);
			break;
		}
		audio->str_cfg.buffer_size = FRAME_SIZE;
		audio->str_cfg.buffer_count = FRAME_NUM;
		rc = 0;
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_GET_CONFIG: {
		struct msm_audio_config cfg;
		mutex_lock(&audio->lock);
		if (copy_to_user((void *)arg, &audio->pcm_cfg, sizeof(cfg))) {
			pr_err(
				"%s: copy_to_user for AUDIO_GET_CONFIG failed\n",
				__func__);
			rc = -EFAULT;
		}
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_SET_CONFIG: {
		struct msm_audio_config config;
		pr_err("%s[%p]:AUDIO_SET_CONFIG\n", __func__, audio);
		mutex_lock(&audio->lock);
		if (copy_from_user(&config, (void *)arg, sizeof(config))) {
			pr_err(
				"%s: copy_from_user for AUDIO_SET_CONFIG failed\n",
				__func__);
			rc = -EFAULT;
			mutex_unlock(&audio->lock);
			break;
		}
		if (audio->feedback != NON_TUNNEL_MODE) {
			pr_err("%s[%p]:Not sufficient permission to change the playback mode\n",
				 __func__, audio);
			rc = -EACCES;
			mutex_unlock(&audio->lock);
			break;
		}
		if ((config.buffer_count > PCM_BUF_COUNT) ||
			(config.buffer_count == 1))
			config.buffer_count = PCM_BUF_COUNT;

		if (config.buffer_size < PCM_BUFSZ_MIN)
			config.buffer_size = PCM_BUFSZ_MIN;

		audio->pcm_cfg.buffer_count = config.buffer_count;
		audio->pcm_cfg.buffer_size = config.buffer_size;
		audio->pcm_cfg.channel_count = config.channel_count;
		audio->pcm_cfg.sample_rate = config.sample_rate;
		rc = 0;
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_SET_BUF_CFG: {
		struct msm_audio_buf_cfg  cfg;
		mutex_lock(&audio->lock);
		if (copy_from_user(&cfg, (void *)arg, sizeof(cfg))) {
			pr_err(
				"%s: copy_from_user for AUDIO_GET_BUF CONFIG failed\n",
				__func__);
			rc = -EFAULT;
			mutex_unlock(&audio->lock);
			break;
		}
		if ((audio->feedback == NON_TUNNEL_MODE) &&
			!cfg.meta_info_enable) {
			rc = -EFAULT;
			mutex_unlock(&audio->lock);
			break;
		}

		audio->buf_cfg.meta_info_enable = cfg.meta_info_enable;
		pr_debug("%s[%p]:session id %d: Set-buf-cfg: meta[%d]",
				__func__, audio,
				audio->ac->session, cfg.meta_info_enable);
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_GET_BUF_CFG: {
		pr_debug("%s[%p]:session id %d: Get-buf-cfg: meta[%d] framesperbuf[%d]\n",
			 __func__, audio,
			audio->ac->session, audio->buf_cfg.meta_info_enable,
			audio->buf_cfg.frames_per_buf);

		mutex_lock(&audio->lock);
		if (copy_to_user((void *)arg, &audio->buf_cfg,
			sizeof(struct msm_audio_buf_cfg))) {
			pr_err(
				"%s: copy_to_user for AUDIO_GET_BUF_CONFIG failed\n",
				__func__);
			rc = -EFAULT;
		}
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_REGISTER_ION: {
		struct msm_audio_ion_info info;
		pr_debug("%s[%p]:AUDIO_REGISTER_ION\n", __func__, audio);
		mutex_lock(&audio->lock);
		if (copy_from_user(&info, (void *)arg, sizeof(info))) {
			pr_err(
				"%s: copy_from_user for AUDIO_REGISTER_ION failed\n",
				__func__);
			rc = -EFAULT;
		} else {
			rc = audio_aio_ion_add(audio, &info);
		}
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_DEREGISTER_ION: {
		struct msm_audio_ion_info info;
		mutex_lock(&audio->lock);
		pr_debug("%s[%p]:AUDIO_DEREGISTER_ION\n", __func__, audio);
		if (copy_from_user(&info, (void *)arg, sizeof(info))) {
			pr_err(
				"%s: copy_from_user for AUDIO_DEREGISTER_ION failed\n",
				__func__);
			rc = -EFAULT;
		} else {
			rc = audio_aio_ion_remove(audio, &info);
		}
		mutex_unlock(&audio->lock);
		break;
	}
	default:
		pr_err("%s: Unknown ioctl cmd = %d", __func__, cmd);
		rc =  -EINVAL;
	}
	return rc;
}

#ifdef CONFIG_COMPAT
struct msm_audio_stream_config32 {
	u32 buffer_size;
	u32 buffer_count;
};

struct msm_audio_stats32 {
	u32 byte_count;
	u32 sample_count;
	u32 unused[2];
};

struct msm_audio_config32 {
	u32 buffer_size;
	u32 buffer_count;
	u32 channel_count;
	u32 sample_rate;
	u32 type;
	u32 meta_field;
	u32 bits;
	u32 unused[3];
};

struct msm_audio_buf_cfg32 {
	u32 meta_info_enable;
	u32 frames_per_buf;
};

struct msm_audio_ion_info32 {
	int fd;
	compat_uptr_t vaddr;
};

enum {
	AUDIO_GET_CONFIG_32 = _IOR(AUDIO_IOCTL_MAGIC, 3,
			struct msm_audio_config32),
	AUDIO_SET_CONFIG_32 = _IOW(AUDIO_IOCTL_MAGIC, 4,
			struct msm_audio_config32),
	AUDIO_GET_STATS_32 = _IOR(AUDIO_IOCTL_MAGIC, 5,
			struct msm_audio_stats32),
	AUDIO_GET_EVENT_32 = _IOR(AUDIO_IOCTL_MAGIC, 13,
			struct msm_audio_event32),
	AUDIO_ASYNC_WRITE_32 = _IOW(AUDIO_IOCTL_MAGIC, 17,
			struct msm_audio_aio_buf32),
	AUDIO_ASYNC_READ_32 = _IOW(AUDIO_IOCTL_MAGIC, 18,
			struct msm_audio_aio_buf32),
	AUDIO_SET_STREAM_CONFIG_32 = _IOW(AUDIO_IOCTL_MAGIC, 80,
			struct msm_audio_stream_config32),
	AUDIO_GET_STREAM_CONFIG_32 = _IOR(AUDIO_IOCTL_MAGIC, 81,
			struct msm_audio_stream_config32),
	AUDIO_GET_BUF_CFG_32 = _IOW(AUDIO_IOCTL_MAGIC, 93,
			struct msm_audio_buf_cfg32),
	AUDIO_SET_BUF_CFG_32 = _IOW(AUDIO_IOCTL_MAGIC, 94,
			struct msm_audio_buf_cfg32),
	AUDIO_REGISTER_ION_32 = _IOW(AUDIO_IOCTL_MAGIC, 97,
			struct msm_audio_ion_info32),
	AUDIO_DEREGISTER_ION_32 = _IOW(AUDIO_IOCTL_MAGIC, 98,
			struct msm_audio_ion_info32),
};

static long audio_aio_compat_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	struct q6audio_aio *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_ABORT_GET_EVENT:
	case AUDIO_OUTPORT_FLUSH:
	case AUDIO_STOP:
	case AUDIO_PAUSE:
	case AUDIO_FLUSH:
	case AUDIO_GET_SESSION_ID:
	case AUDIO_PM_AWAKE:
	case AUDIO_PM_RELAX:
		rc = audio_aio_shared_ioctl(file, cmd, arg);
		break;
	case AUDIO_GET_STATS_32: {
		struct msm_audio_stats32 stats;
		uint64_t timestamp;
		memset(&stats, 0, sizeof(struct msm_audio_stats32));
		stats.byte_count = atomic_read(&audio->in_bytes);
		stats.sample_count = atomic_read(&audio->in_samples);
		rc = q6asm_get_session_time(audio->ac, &timestamp);
		if (rc >= 0)
			memcpy(&stats.unused[0], &timestamp, sizeof(timestamp));
		else
			pr_debug("Error while getting timestamp\n");
		if (copy_to_user((void *)arg, &stats, sizeof(stats))) {
			pr_err(
				"%s: copy_to_user for AUDIO_GET_STATS_32 failed\n",
				__func__);
			rc = -EFAULT;
		}
		break;
	}
	case AUDIO_GET_EVENT_32: {
		pr_debug("%s[%p]:AUDIO_GET_EVENT\n", __func__, audio);
		if (mutex_trylock(&audio->get_event_lock)) {
			rc = audio_aio_process_event_req_compat(audio,
						(void __user *)arg);
			mutex_unlock(&audio->get_event_lock);
		} else
			rc = -EBUSY;
		break;
	}
	case AUDIO_ASYNC_WRITE_32: {
		mutex_lock(&audio->write_lock);
		if (audio->drv_status & ADRV_STATUS_FSYNC)
			rc = -EBUSY;
		else {
			if (audio->enabled)
				rc = audio_aio_buf_add_compat(audio, 1,
						(void __user *)arg);
			else
				rc = -EPERM;
		}
		mutex_unlock(&audio->write_lock);
		break;
	}
	case AUDIO_ASYNC_READ_32: {
		mutex_lock(&audio->read_lock);
		if (audio->feedback)
			rc = audio_aio_buf_add_compat(audio, 0,
					(void __user *)arg);
		else
			rc = -EPERM;
		mutex_unlock(&audio->read_lock);
		break;
	}

	case AUDIO_GET_STREAM_CONFIG_32: {
		struct msm_audio_stream_config32 cfg;
		mutex_lock(&audio->lock);
		memset(&cfg, 0, sizeof(cfg));
		cfg.buffer_size = audio->str_cfg.buffer_size;
		cfg.buffer_count = audio->str_cfg.buffer_count;
		pr_debug("%s[%p]:GET STREAM CFG %d %d\n",
			__func__, audio, cfg.buffer_size, cfg.buffer_count);
		if (copy_to_user((void *)arg, &cfg, sizeof(cfg))) {
			pr_err("%s: copy_to_user for AUDIO_GET_STREAM_CONFIG_32 failed\n",
				__func__);
			rc = -EFAULT;
		}
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_SET_STREAM_CONFIG_32: {
		struct msm_audio_stream_config32 cfg_32;
		struct msm_audio_stream_config cfg;
		pr_debug("%s[%p]:SET STREAM CONFIG\n", __func__, audio);
		mutex_lock(&audio->lock);
		if (copy_from_user(&cfg_32, (void *)arg, sizeof(cfg_32))) {
			pr_err("%s: copy_from_user for AUDIO_SET_STREAM_CONFIG_32 failed\n",
				__func__);
			rc = -EFAULT;
			mutex_unlock(&audio->lock);
			break;
		}
		cfg.buffer_size = cfg_32.buffer_size;
		cfg.buffer_count = cfg_32.buffer_count;

		audio->str_cfg.buffer_size = FRAME_SIZE;
		audio->str_cfg.buffer_count = FRAME_NUM;
		rc = 0;
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_GET_CONFIG_32: {
		struct msm_audio_config32 cfg_32;
		mutex_lock(&audio->lock);
		cfg_32.buffer_size = audio->pcm_cfg.buffer_size;
		cfg_32.buffer_count = audio->pcm_cfg.buffer_count;
		cfg_32.channel_count = audio->pcm_cfg.channel_count;
		cfg_32.sample_rate = audio->pcm_cfg.sample_rate;
		cfg_32.type = audio->pcm_cfg.type;
		cfg_32.meta_field = audio->pcm_cfg.meta_field;
		cfg_32.bits = audio->pcm_cfg.bits;

		if (copy_to_user((void *)arg, &cfg_32, sizeof(cfg_32))) {
			pr_err("%s: copy_to_user for AUDIO_GET_CONFIG_32 failed\n",
				__func__);
			rc = -EFAULT;
		}
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_SET_CONFIG_32: {
		struct msm_audio_config config;
		struct msm_audio_config32 config_32;
		mutex_lock(&audio->lock);

		if (audio->feedback != NON_TUNNEL_MODE) {
			pr_err("%s[%p]:Not sufficient permission to change the playback mode\n",
				 __func__, audio);
			rc = -EACCES;
			mutex_unlock(&audio->lock);
			break;
		}
		pr_err("%s[%p]:AUDIO_SET_CONFIG\n", __func__, audio);
		if (copy_from_user(&config_32, (void *)arg,
					sizeof(config_32))) {
			pr_err("%s: copy_from_user for AUDIO_SET_CONFIG_32 failed\n",
				__func__);
			rc = -EFAULT;
			mutex_unlock(&audio->lock);
			break;
		}
		config.buffer_size = config_32.buffer_size;
		config.buffer_count = config_32.buffer_count;
		config.channel_count = config_32.channel_count;
		config.sample_rate = config_32.sample_rate;
		config.type = config_32.type;
		config.meta_field = config_32.meta_field;
		config.bits = config_32.bits;

		if ((config.buffer_count > PCM_BUF_COUNT) ||
			(config.buffer_count == 1))
			config.buffer_count = PCM_BUF_COUNT;

		if (config.buffer_size < PCM_BUFSZ_MIN)
			config.buffer_size = PCM_BUFSZ_MIN;

		audio->pcm_cfg.buffer_count = config.buffer_count;
		audio->pcm_cfg.buffer_size = config.buffer_size;
		audio->pcm_cfg.channel_count = config.channel_count;
		audio->pcm_cfg.sample_rate = config.sample_rate;
		rc = 0;
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_SET_BUF_CFG_32: {
		struct msm_audio_buf_cfg cfg;
		struct msm_audio_buf_cfg32 cfg_32;
		mutex_lock(&audio->lock);
		if (copy_from_user(&cfg_32, (void *)arg, sizeof(cfg_32))) {
			pr_err("%s: copy_from_user for AUDIO_SET_CONFIG_32 failed\n",
				__func__);
			rc = -EFAULT;
			mutex_unlock(&audio->lock);
			break;
		}
		cfg.meta_info_enable = cfg_32.meta_info_enable;
		cfg.frames_per_buf = cfg_32.frames_per_buf;

		if ((audio->feedback == NON_TUNNEL_MODE) &&
			!cfg.meta_info_enable) {
			rc = -EFAULT;
			mutex_unlock(&audio->lock);
			break;
		}

		audio->buf_cfg.meta_info_enable = cfg.meta_info_enable;
		pr_debug("%s[%p]:session id %d: Set-buf-cfg: meta[%d]",
				__func__, audio,
				audio->ac->session, cfg.meta_info_enable);
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_GET_BUF_CFG_32: {
		struct msm_audio_buf_cfg32 cfg_32;
		pr_debug("%s[%p]:session id %d: Get-buf-cfg: meta[%d] framesperbuf[%d]\n",
			 __func__, audio,
			audio->ac->session, audio->buf_cfg.meta_info_enable,
			audio->buf_cfg.frames_per_buf);

		mutex_lock(&audio->lock);
		cfg_32.meta_info_enable = audio->buf_cfg.meta_info_enable;
		cfg_32.frames_per_buf = audio->buf_cfg.frames_per_buf;
		if (copy_to_user((void *)arg, &cfg_32,
			sizeof(struct msm_audio_buf_cfg32))) {
			pr_err("%s: copy_to_user for AUDIO_GET_BUF_CFG_32 failed\n",
				__func__);
			rc = -EFAULT;
		}
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_REGISTER_ION_32: {
		struct msm_audio_ion_info32 info_32;
		struct msm_audio_ion_info info;
		pr_debug("%s[%p]:AUDIO_REGISTER_ION\n", __func__, audio);
		mutex_lock(&audio->lock);
		if (copy_from_user(&info_32, (void *)arg, sizeof(info_32))) {
			pr_err("%s: copy_from_user for AUDIO_REGISTER_ION_32 failed\n",
				__func__);
			rc = -EFAULT;
		} else {
			info.fd = info_32.fd;
			info.vaddr = compat_ptr(info_32.vaddr);
			rc = audio_aio_ion_add(audio, &info);
		}
		mutex_unlock(&audio->lock);
		break;
	}
	case AUDIO_DEREGISTER_ION_32: {
		struct msm_audio_ion_info32 info_32;
		struct msm_audio_ion_info info;
		mutex_lock(&audio->lock);
		pr_debug("%s[%p]:AUDIO_DEREGISTER_ION\n", __func__, audio);
		if (copy_from_user(&info_32, (void *)arg, sizeof(info_32))) {
			pr_err("%s: copy_from_user for	AUDIO_DEREGISTER_ION_32 failed\n",
				__func__);
			rc = -EFAULT;
		} else {
			info.fd = info_32.fd;
			info.vaddr = compat_ptr(info_32.vaddr);
			rc = audio_aio_ion_remove(audio, &info);
		}
		mutex_unlock(&audio->lock);
		break;
	}
	default:
		pr_err("%s: Unknown ioctl cmd = %d", __func__, cmd);
		rc =  -EINVAL;
	}
	return rc;
}
#endif
