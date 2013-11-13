/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/memory_alloc.h>
#include <asm/mach-types.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/ocmem.h>
#include <mach/subsystem_notif.h>
#include <mach/subsystem_restart.h>
#include <mach/msm_memtypes.h>
#include <mach/ramdump.h>
#include "q6core.h"
#include "audio_ocmem.h"


#define AUDIO_OCMEM_BUF_SIZE (512 * SZ_1K)

/**
 * Exercise OCMEM Dump if audio OCMEM state is
 * one of the following. All other states indicate
 * audio data is not mapped from DDR to OCMEM and
 * therefore no need of dump.
 */
#define _DO_OCMEM_DUMP_BIT_MASK_\
		((1 << OCMEM_STATE_MAP_COMPL) |\
		(1 << OCMEM_STATE_MAP_TRANSITION) |\
		(1 << OCMEM_STATE_UNMAP_TRANSITION) |\
		(1 << OCMEM_STATE_SHRINK) |\
		(1 << OCMEM_STATE_GROW))

/**
 * Wait for OCMEM driver to process and respond for
 * ongoing map/unmap request before calling OCMEM dump.
 */
#define _WAIT_BFR_DUMP_BIT_MASK_\
		((1 << OCMEM_STATE_MAP_COMPL) |\
		(1 << OCMEM_STATE_UNMAP_COMPL) |\
		(1 << OCMEM_STATE_MAP_FAIL) |\
		(1 << OCMEM_STATE_UNMAP_FAIL))

#define _MAP_RESPONSE_BIT_MASK_\
		((1 << OCMEM_STATE_MAP_COMPL) |\
		(1 << OCMEM_STATE_MAP_FAIL))


#define _UNMAP_RESPONSE_BIT_MASK_\
		((1 << OCMEM_STATE_UNMAP_COMPL) |\
		(1 << OCMEM_STATE_UNMAP_FAIL))

#define _BIT_MASK_\
		((1 << OCMEM_STATE_SSR) |\
		(1 << OCMEM_STATE_EXIT) |\
		(1 << OCMEM_STATE_GROW) |\
		(1 << OCMEM_STATE_SHRINK))

#define set_bit_pos(x, y)  (atomic_set(&x, (atomic_read(&x) | (1 << y))))
#define clear_bit_pos(x, y)  (atomic_set(&x, (atomic_read(&x) & (~(1 << y)))))
#define test_bit_pos(x, y) ((atomic_read(&x)) & (1 << y))

static int enable_ocmem_audio_voice = 1;
module_param(enable_ocmem_audio_voice, int,
			S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(enable_ocmem_audio_voice, "control OCMEM usage for audio/voice");

enum {
	OCMEM_STATE_DEFAULT = 0,
	OCMEM_STATE_ALLOC = 1,
	OCMEM_STATE_MAP_TRANSITION,
	OCMEM_STATE_MAP_COMPL,
	OCMEM_STATE_UNMAP_TRANSITION,
	OCMEM_STATE_UNMAP_COMPL,
	OCMEM_STATE_SHRINK,
	OCMEM_STATE_GROW,
	OCMEM_STATE_FREE,
	OCMEM_STATE_MAP_FAIL,
	OCMEM_STATE_UNMAP_FAIL,
	OCMEM_STATE_EXIT,
	OCMEM_STATE_SSR,
	OCMEM_STATE_DISABLE,
};
static void audio_ocmem_process_workdata(struct work_struct *work);

struct audio_ocmem_workdata {
	int	id;
	bool    en;
	struct work_struct work;
};

struct voice_ocmem_workdata {
	int id;
	bool en;
	struct work_struct work;
};

struct audio_ocmem_prv {
	atomic_t audio_state;
	struct ocmem_notifier *audio_hdl;
	struct ocmem_buf *buf;
	uint32_t audio_ocmem_bus_client;
	struct ocmem_map_list mlist;
	struct avcs_cmd_rsp_get_low_power_segments_info_t *lp_memseg_ptr;
	wait_queue_head_t audio_wait;
	atomic_t  audio_cond;
	atomic_t  audio_exit;
	spinlock_t audio_lock;
	struct mutex state_process_lock;
	struct workqueue_struct *audio_ocmem_workqueue;
	struct workqueue_struct *voice_ocmem_workqueue;
	bool ocmem_en;
	bool audio_ocmem_running;
	void *ocmem_ramdump_dev;
	struct ramdump_segment ocmem_ramdump_segment;
	unsigned long ocmem_dump_addr;
};

static struct audio_ocmem_prv audio_ocmem_lcl;

static int audio_ocmem_client_cb(struct notifier_block *this,
		 unsigned long event1, void *data)
{
	int rc = NOTIFY_DONE;
	unsigned long flags;
	struct ocmem_buf *rbuf;
	int vwait = 0;

	pr_debug("%s: event[%ld] cur state[%x]\n", __func__,
			event1, atomic_read(&audio_ocmem_lcl.audio_state));

	spin_lock_irqsave(&audio_ocmem_lcl.audio_lock, flags);
	switch (event1) {
	case OCMEM_MAP_DONE:
		pr_debug("%s: map done\n", __func__);
		clear_bit_pos(audio_ocmem_lcl.audio_state,
				OCMEM_STATE_MAP_TRANSITION);
		set_bit_pos(audio_ocmem_lcl.audio_state, OCMEM_STATE_MAP_COMPL);
		break;
	case OCMEM_MAP_FAIL:
		pr_debug("%s: map fail\n", __func__);
		clear_bit_pos(audio_ocmem_lcl.audio_state,
				OCMEM_STATE_MAP_TRANSITION);
		set_bit_pos(audio_ocmem_lcl.audio_state, OCMEM_STATE_MAP_FAIL);
		break;
	case OCMEM_UNMAP_DONE:
		pr_debug("%s: unmap done\n", __func__);
		clear_bit_pos(audio_ocmem_lcl.audio_state,
				OCMEM_STATE_UNMAP_TRANSITION);
		set_bit_pos(audio_ocmem_lcl.audio_state,
				OCMEM_STATE_UNMAP_COMPL);
		break;
	case OCMEM_UNMAP_FAIL:
		pr_debug("%s: unmap fail\n", __func__);
		clear_bit_pos(audio_ocmem_lcl.audio_state,
				OCMEM_STATE_UNMAP_TRANSITION);
		set_bit_pos(audio_ocmem_lcl.audio_state,
			    OCMEM_STATE_UNMAP_FAIL);
		break;
	case OCMEM_ALLOC_GROW:
		rbuf = data;
		if ((rbuf->len == AUDIO_OCMEM_BUF_SIZE)) {
			audio_ocmem_lcl.buf = data;
			pr_debug("%s: Alloc grow request received buf->addr: 0x%08lx\n",
						__func__,
						(audio_ocmem_lcl.buf)->addr);
			set_bit_pos(audio_ocmem_lcl.audio_state,
					OCMEM_STATE_GROW);
		} else {
			pr_debug("%s: Alloc grow request with size: %ld",
							__func__,
							rbuf->len);
			vwait = 1;
		}

		break;
	case OCMEM_ALLOC_SHRINK:
		pr_debug("%s: Alloc shrink request received\n", __func__);
		set_bit_pos(audio_ocmem_lcl.audio_state, OCMEM_STATE_SHRINK);
		break;
	default:
		pr_err("%s: Invalid event[%ld]\n", __func__, event1);
		break;
	}
	spin_unlock_irqrestore(&audio_ocmem_lcl.audio_lock, flags);
		atomic_set(&audio_ocmem_lcl.audio_cond, 0);
		wake_up(&audio_ocmem_lcl.audio_wait);
	return rc;
}
int get_state_to_process(atomic_t *state)
{

	if (test_bit_pos((*state), OCMEM_STATE_SHRINK)) {
		pr_debug("%s: returning shrink state\n", __func__);
		return OCMEM_STATE_SHRINK;
	} else if (test_bit_pos((*state), OCMEM_STATE_GROW)) {
		pr_debug("%s: returning grow state\n", __func__);
		return OCMEM_STATE_GROW;
	} else if (test_bit_pos((*state), OCMEM_STATE_EXIT)) {
		pr_debug("%s: returning exit state\n", __func__);
		return OCMEM_STATE_EXIT;
	} else if (test_bit_pos((*state), OCMEM_STATE_SSR)) {
		pr_debug("%s: returning ssr state\n", __func__);
		return OCMEM_STATE_SSR;
	} else
		return -EINVAL;

}

/**
 * audio_ocmem_enable() - Exercise OCMEM for audio
 * @cid:	client id - OCMEM_LP_AUDIO
 *
 * OCMEM gets allocated for audio usecase and the low power
 * segments obtained from the DSP will be moved from/to main
 * memory to OCMEM. Shrink and grow requests will be received
 * and processed accordingly based on the current audio state.
 */
int audio_ocmem_enable(int cid)
{
	int ret;
	int i, j;
	int state_bit;
	struct ocmem_buf *buf = NULL;
	struct avcs_cmd_rsp_get_low_power_segments_info_t *lp_segptr;

	pr_debug("%s, %p\n", __func__, &audio_ocmem_lcl);
	atomic_set(&audio_ocmem_lcl.audio_state, OCMEM_STATE_DEFAULT);
	if (audio_ocmem_lcl.lp_memseg_ptr == NULL) {
		/* Retrieve low power segments */
		ret = core_get_low_power_segments(
					&audio_ocmem_lcl.lp_memseg_ptr);
		if (ret != 0) {
			pr_err("%s: get low power segments from DSP failed, rc=%d\n",
					__func__, ret);
			mutex_unlock(&audio_ocmem_lcl.state_process_lock);
			goto fail_cmd;
		}
	}
	lp_segptr = audio_ocmem_lcl.lp_memseg_ptr;
	audio_ocmem_lcl.mlist.num_chunks = lp_segptr->num_segments;
	for (i = 0, j = 0; j < audio_ocmem_lcl.mlist.num_chunks; j++, i++) {
		audio_ocmem_lcl.mlist.chunks[j].ro =
			(lp_segptr->mem_segment[i].type == READ_ONLY_SEGMENT);
		audio_ocmem_lcl.mlist.chunks[j].ddr_paddr =
			lp_segptr->mem_segment[i].start_address_lsw;
		audio_ocmem_lcl.mlist.chunks[j].size =
			lp_segptr->mem_segment[i].size;
		pr_debug("%s: ro:%d, ddr_paddr[0x%08x], size[0x%x]\n", __func__,
			audio_ocmem_lcl.mlist.chunks[j].ro,
			(uint32_t)audio_ocmem_lcl.mlist.chunks[j].ddr_paddr,
			(uint32_t)audio_ocmem_lcl.mlist.chunks[j].size);
	}
	/* Non-blocking ocmem allocate (asynchronous) */
	buf = ocmem_allocate_nb(cid, AUDIO_OCMEM_BUF_SIZE);
	if (IS_ERR_OR_NULL(buf)) {
		pr_err("%s: failed: %d\n", __func__, cid);
		ret = -ENOMEM;
		mutex_unlock(&audio_ocmem_lcl.state_process_lock);
		goto fail_cmd;
	}

	set_bit_pos(audio_ocmem_lcl.audio_state, OCMEM_STATE_ALLOC);

	audio_ocmem_lcl.buf = buf;
	atomic_set(&audio_ocmem_lcl.audio_exit, 0);
	atomic_set(&audio_ocmem_lcl.audio_cond, 1);
	pr_debug("%s: buf->len: %ld\n", __func__, buf->len);
	if (!buf->len) {
		pr_debug("%s: buf.len is 0, waiting for ocmem region\n",
								__func__);
		mutex_unlock(&audio_ocmem_lcl.state_process_lock);
		wait_event_interruptible(audio_ocmem_lcl.audio_wait,
			(atomic_read(&audio_ocmem_lcl.audio_cond) == 0)	||
			(atomic_read(&audio_ocmem_lcl.audio_exit) == 1));
		if (atomic_read(&audio_ocmem_lcl.audio_exit)) {
			ret = ocmem_free(OCMEM_LP_AUDIO, audio_ocmem_lcl.buf);
			if (ret) {
				pr_err("%s: ocmem_free failed, state[%d]\n",
				__func__,
				atomic_read(&audio_ocmem_lcl.audio_state));
			}
			pr_info("%s: audio playback ended while waiting for ocmem\n",
					__func__);
			ret = 0;
			goto fail_cmd;
		}
		clear_bit_pos(audio_ocmem_lcl.audio_state, OCMEM_STATE_GROW);
		mutex_trylock(&audio_ocmem_lcl.state_process_lock);
	}
	pr_debug("%s: buf->len: %ld\n", __func__, (audio_ocmem_lcl.buf)->len);

	/* vote for ocmem bus bandwidth */
	ret = msm_bus_scale_client_update_request(
				audio_ocmem_lcl.audio_ocmem_bus_client,
				1);
	if (ret)
		pr_err("%s: failed to vote for bus bandwidth\n", __func__);

	set_bit_pos(audio_ocmem_lcl.audio_state, OCMEM_STATE_MAP_TRANSITION);

	pr_debug("%s: buf->addr: 0x%08lx, len: %ld, audio_state[0x%x]\n",
				__func__,
				audio_ocmem_lcl.buf->addr,
				audio_ocmem_lcl.buf->len,
				atomic_read(&audio_ocmem_lcl.audio_state));

	atomic_set(&audio_ocmem_lcl.audio_cond, 1);
	ret = ocmem_map(cid, audio_ocmem_lcl.buf, &audio_ocmem_lcl.mlist);
	if (ret) {
		pr_err("%s: ocmem_map failed\n", __func__);
		atomic_set(&audio_ocmem_lcl.audio_state, OCMEM_STATE_MAP_FAIL);
		goto fail_cmd1;
	}

	wait_event_interruptible(audio_ocmem_lcl.audio_wait,
			(atomic_read(&audio_ocmem_lcl.audio_state) &
					_MAP_RESPONSE_BIT_MASK_) != 0);
	atomic_set(&audio_ocmem_lcl.audio_cond, 1);

	mutex_unlock(&audio_ocmem_lcl.state_process_lock);
	pr_debug("%s: audio_cond[%d] audio_state[0x%x]\n", __func__,
				atomic_read(&audio_ocmem_lcl.audio_cond),
				atomic_read(&audio_ocmem_lcl.audio_state));

	while ((test_bit_pos(audio_ocmem_lcl.audio_state,
					OCMEM_STATE_DISABLE)) == 0) {

		wait_event_interruptible(audio_ocmem_lcl.audio_wait,
				(atomic_read(&audio_ocmem_lcl.audio_state) &
						_BIT_MASK_) != 0);
		mutex_lock(&audio_ocmem_lcl.state_process_lock);
		state_bit = get_state_to_process(&audio_ocmem_lcl.audio_state);
		switch (state_bit) {
		case OCMEM_STATE_MAP_COMPL:
			pr_debug("%s: audio_cond[0x%x], audio_state[0x%x]\n",
			__func__, atomic_read(&audio_ocmem_lcl.audio_cond),
			atomic_read(&audio_ocmem_lcl.audio_state));
			atomic_set(&audio_ocmem_lcl.audio_state,
					OCMEM_STATE_MAP_COMPL);
			atomic_set(&audio_ocmem_lcl.audio_cond, 1);
			break;
		case OCMEM_STATE_SHRINK:
			pr_debug("%s: ocmem shrink request process\n",
							__func__);
			atomic_set(&audio_ocmem_lcl.audio_cond, 1);
			clear_bit_pos(audio_ocmem_lcl.audio_state,
					OCMEM_STATE_MAP_COMPL);
			set_bit_pos(audio_ocmem_lcl.audio_state,
					OCMEM_STATE_UNMAP_TRANSITION);
			ret = ocmem_unmap(cid, audio_ocmem_lcl.buf,
					&audio_ocmem_lcl.mlist);
			if (ret) {
				pr_err("%s: ocmem_unmap failed, state[%d]\n",
				__func__,
				atomic_read(&audio_ocmem_lcl.audio_state));
				goto fail_cmd1;
			}

			wait_event_interruptible(audio_ocmem_lcl.audio_wait,
				(atomic_read(&audio_ocmem_lcl.audio_state) &
					     _UNMAP_RESPONSE_BIT_MASK_)
					     != 0);
			ret = ocmem_shrink(cid, audio_ocmem_lcl.buf, 0);
			if (ret) {
				pr_err("%s: ocmem_shrink failed, state[%d]\n",
				__func__,
				atomic_read(&audio_ocmem_lcl.audio_state));
				goto fail_cmd1;
			}
			atomic_set(&audio_ocmem_lcl.audio_cond, 1);
			clear_bit_pos(audio_ocmem_lcl.audio_state,
					OCMEM_STATE_SHRINK);
			pr_debug("%s:shrink process complete\n", __func__);
			break;
		case OCMEM_STATE_GROW:
			pr_debug("%s: ocmem grow request process\n",
							__func__);
			atomic_set(&audio_ocmem_lcl.audio_cond, 1);
			clear_bit_pos(audio_ocmem_lcl.audio_state,
					OCMEM_STATE_UNMAP_COMPL);
			set_bit_pos(audio_ocmem_lcl.audio_state,
					OCMEM_STATE_MAP_TRANSITION);
			ret = ocmem_map(cid, audio_ocmem_lcl.buf,
						&audio_ocmem_lcl.mlist);
			if (ret) {
				pr_err("%s: ocmem_map failed, state[%d]\n",
				__func__,
				atomic_read(&audio_ocmem_lcl.audio_state));
				goto fail_cmd1;
			}
			wait_event_interruptible(audio_ocmem_lcl.audio_wait,
				(atomic_read(&audio_ocmem_lcl.audio_state) &
						_MAP_RESPONSE_BIT_MASK_) != 0);

			clear_bit_pos(audio_ocmem_lcl.audio_state,
					OCMEM_STATE_GROW);
			atomic_set(&audio_ocmem_lcl.audio_cond, 1);
			break;
		case OCMEM_STATE_EXIT:
			if (test_bit_pos(audio_ocmem_lcl.audio_state,
						OCMEM_STATE_MAP_COMPL)) {
				clear_bit_pos(audio_ocmem_lcl.audio_state,
					OCMEM_STATE_MAP_COMPL);
				set_bit_pos(audio_ocmem_lcl.audio_state,
					OCMEM_STATE_UNMAP_TRANSITION);
				ret = ocmem_unmap(cid, audio_ocmem_lcl.buf,
						&audio_ocmem_lcl.mlist);
				if (ret) {
					pr_err("%s: ocmem_unmap failed, state[0x%x]\n",
					__func__,
				atomic_read(&audio_ocmem_lcl.audio_state));
					goto fail_cmd1;
				}
				wait_event_interruptible(
				audio_ocmem_lcl.audio_wait,
				(atomic_read(&audio_ocmem_lcl.audio_state) &
				_UNMAP_RESPONSE_BIT_MASK_) != 0);
			}

			if (test_bit_pos(audio_ocmem_lcl.audio_state,
						OCMEM_STATE_SHRINK)) {
				pr_debug("%s: SHRINK while exiting\n",
								__func__);
				ret = ocmem_shrink(cid, audio_ocmem_lcl.buf,
									0);
				if (ret) {
					pr_err("%s: ocmem_shrink failed, state[0x%x]\n",
						__func__,
				atomic_read(&audio_ocmem_lcl.audio_state));
					goto fail_cmd1;
				}
				clear_bit_pos(audio_ocmem_lcl.audio_state,
						OCMEM_STATE_SHRINK);

			}

			if (test_bit_pos(audio_ocmem_lcl.audio_state,
						OCMEM_STATE_DISABLE) ||
			    test_bit_pos(audio_ocmem_lcl.audio_state,
			     OCMEM_STATE_FREE)) {
				pr_info("%s: audio already freed from ocmem, state[0x%x]\n",
					__func__,
				atomic_read(&audio_ocmem_lcl.audio_state));
				goto fail_cmd2;
			}
			pr_debug("%s: calling ocmem free, state:0x%x\n",
				__func__,
				atomic_read(&audio_ocmem_lcl.audio_state));
			ret = ocmem_free(OCMEM_LP_AUDIO, audio_ocmem_lcl.buf);
			if (ret == -EAGAIN) {
				pr_debug("%s: received EAGAIN\n", __func__);
				if (test_bit_pos(audio_ocmem_lcl.audio_state,
							OCMEM_STATE_SHRINK)) {
					ret = ocmem_shrink(cid,
							audio_ocmem_lcl.buf,
							0);
					if (ret) {
						pr_err("%s: ocmem_shrink failed, state[0x%x]\n",
							__func__,
				atomic_read(&audio_ocmem_lcl.audio_state));
							goto fail_cmd1;
					}
					pr_debug("calling free after EAGAIN");
					ret = ocmem_free(OCMEM_LP_AUDIO,
							audio_ocmem_lcl.buf);
					if (ret) {
						pr_err("%s: ocmem_free failed\n",
								__func__);
						goto fail_cmd2;
					}
				} else {
					pr_debug("%s: shrink callback already processed\n",
								__func__);
					goto fail_cmd1;
				}
			} else if (ret) {
				pr_err("%s: ocmem_free failed, state[0x%x], ret:%d\n",
					__func__,
				atomic_read(&audio_ocmem_lcl.audio_state),
				ret);
				goto fail_cmd2;
			}
			set_bit_pos(audio_ocmem_lcl.audio_state,
					OCMEM_STATE_FREE);
			pr_debug("%s: ocmem_free success, state[0x%x]\n",
				 __func__,
				 atomic_read(&audio_ocmem_lcl.audio_state));
		/* Fall through */
		case OCMEM_STATE_SSR:
			msm_bus_scale_client_update_request(
				audio_ocmem_lcl.audio_ocmem_bus_client,
				0);
			set_bit_pos(audio_ocmem_lcl.audio_state,
					OCMEM_STATE_DISABLE);
			break;

		case -EINVAL:
			pr_info("%s: audio_cond[%d] audio_state[0x%x]\n",
				__func__,
				atomic_read(&audio_ocmem_lcl.audio_cond),
				atomic_read(&audio_ocmem_lcl.audio_state));
			break;
		}
		mutex_unlock(&audio_ocmem_lcl.state_process_lock);
	}
	ret = 0;
	goto fail_cmd;

fail_cmd1:
	ret = ocmem_free(OCMEM_LP_AUDIO, audio_ocmem_lcl.buf);
	if (ret)
		pr_err("%s: ocmem_free failed\n", __func__);
	set_bit_pos(audio_ocmem_lcl.audio_state,
		    OCMEM_STATE_FREE);
fail_cmd2:
	mutex_unlock(&audio_ocmem_lcl.state_process_lock);
fail_cmd:
	pr_debug("%s: exit\n", __func__);
	audio_ocmem_lcl.audio_ocmem_running = false;
	return ret;
}

/**
 * audio_ocmem_disable() - Disable OCMEM for audio
 * @cid:	client id - OCMEM_LP_AUDIO
 *
 * OCMEM gets deallocated for audio usecase. Depending on
 * current audio state, OCMEM will be freed from using audio
 * segments.
 */
int audio_ocmem_disable(int cid)
{

	pr_debug("%s: audio_cond[0x%x], audio_state[0x%x]\n", __func__,
			 atomic_read(&audio_ocmem_lcl.audio_cond),
			 atomic_read(&audio_ocmem_lcl.audio_state));
	if (!test_bit_pos(audio_ocmem_lcl.audio_state,
			  OCMEM_STATE_SSR))
		set_bit_pos(audio_ocmem_lcl.audio_state,
			    OCMEM_STATE_EXIT);

	wake_up(&audio_ocmem_lcl.audio_wait);

	mutex_unlock(&audio_ocmem_lcl.state_process_lock);
	pr_debug("%s: exit\n", __func__);
	return 0;
}

static void voice_ocmem_process_workdata(struct work_struct *work)
{
	int cid;
	bool en;
	int rc = 0;

	struct voice_ocmem_workdata *voice_ocm_work =
		container_of(work, struct voice_ocmem_workdata, work);

	en = voice_ocm_work->en;
	switch (voice_ocm_work->id) {
	case VOICE:
		cid = OCMEM_VOICE;
		if (en)
			disable_ocmem_for_voice(cid);
		else
			enable_ocmem_after_voice(cid);
		break;
	default:
		pr_err("%s: Invalid client id[%d]\n", __func__,
					voice_ocm_work->id);
		rc = -EINVAL;
	}

	kfree(voice_ocm_work);
	return;
}
/**
 * voice_ocmem_process_req() - disable/enable OCMEM during voice call
 * @cid:	client id - VOICE
 * @enable:	1 - enable
 *		0 - disable
 *
 * This configures OCMEM during start of voice call. If any
 * audio clients are already using OCMEM, they will be evicted
 * out of OCMEM during voice call and get restored after voice
 * call.
 */
int voice_ocmem_process_req(int cid, bool enable)
{

	struct voice_ocmem_workdata *workdata = NULL;

	if (enable) {
		if (!enable_ocmem_audio_voice)
			audio_ocmem_lcl.ocmem_en = false;
		else
			audio_ocmem_lcl.ocmem_en = true;
	}
	if (audio_ocmem_lcl.ocmem_en) {
		if (audio_ocmem_lcl.voice_ocmem_workqueue == NULL) {
			pr_err("%s: voice ocmem workqueue is NULL\n",
								__func__);
			return -EINVAL;
		}
		workdata = kzalloc(sizeof(struct voice_ocmem_workdata),
							GFP_ATOMIC);
		if (workdata == NULL) {
			pr_err("%s: mem failure\n", __func__);
			return -ENOMEM;
		}
		workdata->id = cid;
		workdata->en = enable;

		INIT_WORK(&workdata->work, voice_ocmem_process_workdata);
		queue_work(audio_ocmem_lcl.voice_ocmem_workqueue,
							&workdata->work);
	}

	return 0;
}

/**
 * disable_ocmem_for_voice() - disable OCMEM during voice call
 * @cid:        client id - OCMEM_VOICE
 *
 * This configures OCMEM during start of voice call. If any
 * audio clients are already using OCMEM, they will be evicted
 */
int disable_ocmem_for_voice(int cid)
{
	int ret;

	ret = ocmem_evict(cid);
	if (ret)
		pr_err("%s: ocmem_evict is not successful\n", __func__);
	return ret;
}

/**
 * enable_ocmem_for_voice() - To enable OCMEM after voice call
 * @cid:	client id - OCMEM_VOICE
 *
 * OCMEM gets re-enabled after OCMEM voice call. If other client
 * is evicted out of OCMEM, that gets restored and remapped in
 * OCMEM after the voice call.
 */
int enable_ocmem_after_voice(int cid)
{
	int ret;

	ret = ocmem_restore(cid);
	if (ret)
		pr_err("%s: ocmem_restore is not successful\n", __func__);
	return ret;
}


static void audio_ocmem_process_workdata(struct work_struct *work)
{
	int cid;
	bool en;
	int rc = 0;

	struct audio_ocmem_workdata *audio_ocm_work =
			container_of(work, struct audio_ocmem_workdata, work);

	en = audio_ocm_work->en;
	mutex_lock(&audio_ocmem_lcl.state_process_lock);
	/* if previous work waiting for ocmem - signal it to exit */
	atomic_set(&audio_ocmem_lcl.audio_exit, 1);
	pr_debug("%s: acquired mutex for %d\n", __func__, en);
	switch (audio_ocm_work->id) {
	case AUDIO:
		cid = OCMEM_LP_AUDIO;
		if (en)
			audio_ocmem_enable(cid);
		else
			audio_ocmem_disable(cid);
		break;
	default:
		pr_err("%s: Invalid client id[%d]\n", __func__,
					audio_ocm_work->id);
		rc = -EINVAL;
	}

	kfree(audio_ocm_work);
	return;
}

/**
 * audio_ocmem_process_req() - process audio request to use OCMEM
 * @id:		client id - OCMEM_LP_AUDIO
 * @enable:	enable or disable OCMEM
 *
 * A workqueue gets created and initialized to use OCMEM for
 * audio clients.
 */
int audio_ocmem_process_req(int id, bool enable)
{
	struct audio_ocmem_workdata *workdata = NULL;

	if (enable) {
		if (!enable_ocmem_audio_voice)
			audio_ocmem_lcl.ocmem_en = false;
		else
			audio_ocmem_lcl.ocmem_en = true;
	}

	if (audio_ocmem_lcl.ocmem_en &&
	    (!enable || !audio_ocmem_lcl.audio_ocmem_running)) {
		if (audio_ocmem_lcl.audio_ocmem_workqueue == NULL) {
			pr_err("%s: audio ocmem workqueue is NULL\n",
								__func__);
			return -EINVAL;
		}
		workdata = kzalloc(sizeof(struct audio_ocmem_workdata),
							GFP_ATOMIC);
		if (workdata == NULL) {
			pr_err("%s: mem failure\n", __func__);
			return -ENOMEM;
		}
		workdata->id = id;
		workdata->en = enable;
		audio_ocmem_lcl.audio_ocmem_running = true;

		INIT_WORK(&workdata->work, audio_ocmem_process_workdata);
		queue_work(audio_ocmem_lcl.audio_ocmem_workqueue,
							&workdata->work);
	}

	return 0;
}


static struct notifier_block audio_ocmem_client_nb = {
	.notifier_call = audio_ocmem_client_cb,
};

static int audio_ocmem_platform_data_populate(struct platform_device *pdev)
{
	struct msm_bus_scale_pdata *audio_ocmem_adata = NULL;

	if (!pdev->dev.of_node) {
		pr_err("%s: device tree information missing\n", __func__);
		return -ENODEV;
	}
	audio_ocmem_adata = msm_bus_cl_get_pdata(pdev);
	if (!audio_ocmem_adata) {
		pr_err("%s: bus device tree allocation failed\n", __func__);
		return -EINVAL;
	}

	dev_set_drvdata(&pdev->dev, audio_ocmem_adata);

	return 0;
}

static void do_ocmem_ramdump(void)
{
	int ret = 0;
	void *virt = NULL;

	virt = ioremap(audio_ocmem_lcl.ocmem_dump_addr, AUDIO_OCMEM_BUF_SIZE);
	ret = ocmem_dump(OCMEM_LP_AUDIO,
			 audio_ocmem_lcl.buf,
			 (unsigned long)virt);
	iounmap(virt);

	if (ret)
		pr_err("%s: ocmem_dump failed\n", __func__);

	audio_ocmem_lcl.ocmem_ramdump_segment.address
			= (unsigned long)audio_ocmem_lcl.ocmem_dump_addr;
	audio_ocmem_lcl.ocmem_ramdump_segment.size
						= AUDIO_OCMEM_BUF_SIZE;
	ret = do_ramdump(audio_ocmem_lcl.ocmem_ramdump_dev,
			 &audio_ocmem_lcl.ocmem_ramdump_segment,
			 1);
	if (ret < 0)
		pr_err("%s: do_ramdump failed\n", __func__);
}

static void process_ocmem_dump(void)
{
	int ret = 0;

	set_bit_pos(audio_ocmem_lcl.audio_state, OCMEM_STATE_SSR);

	if (atomic_read(&audio_ocmem_lcl.audio_state) &
	    _DO_OCMEM_DUMP_BIT_MASK_) {

		wait_event_interruptible(audio_ocmem_lcl.audio_wait,
				(atomic_read(&audio_ocmem_lcl.audio_state) &
				 _WAIT_BFR_DUMP_BIT_MASK_) != 0);

		if (test_bit_pos(audio_ocmem_lcl.audio_state,
				 OCMEM_STATE_MAP_COMPL) ||
		    test_bit_pos(audio_ocmem_lcl.audio_state,
				 OCMEM_STATE_UNMAP_FAIL)) {

			if (audio_ocmem_lcl.ocmem_dump_addr &&
			    audio_ocmem_lcl.ocmem_ramdump_dev)
				do_ocmem_ramdump();
			else
				pr_err("%s: Error calling ocmem ramdump\n",
					__func__);

			ret = ocmem_drop(OCMEM_LP_AUDIO, audio_ocmem_lcl.buf,
					 &audio_ocmem_lcl.mlist);
			if (ret)
				pr_err("%s: ocmem_drop failed\n", __func__);
		}
	}

	ret = ocmem_free(OCMEM_LP_AUDIO, audio_ocmem_lcl.buf);
	if (ret)
		pr_err("%s: ocmem_free failed\n", __func__);
}

static int lpass_notifier_cb(struct notifier_block *this, unsigned long code,
			     void *_cmd)
{
	int ret = NOTIFY_DONE;

	switch (code) {
	case SUBSYS_BEFORE_SHUTDOWN:
	pr_debug("AO-Notify: Shutdown started\n");
		break;
	case SUBSYS_AFTER_SHUTDOWN:
	pr_debug("AO-Notify: Shutdown Completed\n");
		break;
	case SUBSYS_RAMDUMP_NOTIFICATION:
		pr_debug("AO-Notify: OCMEM dump\n");
		if (audio_ocmem_lcl.ocmem_en &&
		    audio_ocmem_lcl.audio_ocmem_running)
			process_ocmem_dump();
		pr_debug("AO-Notify: OCMEM dump done\n");
		break;
	case SUBSYS_BEFORE_POWERUP:
		pr_debug("AO-Notify: Powerup started\n");
		break;
	case SUBSYS_AFTER_POWERUP:
		pr_debug("AO-Notify: Powerup completed\n");
		break;
	default:
		pr_err("AO-Notify: Generel: %lu\n", code);
		break;
	}
	return ret;
}

static struct notifier_block anb = {
	.notifier_call = lpass_notifier_cb,
};

static int ocmem_audio_client_probe(struct platform_device *pdev)
{
	int ret;
	struct msm_bus_scale_pdata *audio_ocmem_bus_scale_pdata = NULL;

	pr_debug("%s\n", __func__);

	audio_ocmem_lcl.audio_hdl = ocmem_notifier_register(OCMEM_LP_AUDIO,
						&audio_ocmem_client_nb);
	if (PTR_RET(audio_ocmem_lcl.audio_hdl) == -EPROBE_DEFER)
		return -EPROBE_DEFER;
	else if (!audio_ocmem_lcl.audio_hdl) {
		pr_err("%s: Failed to get ocmem handle %d\n", __func__,
						OCMEM_LP_AUDIO);
		return -ENODEV;
	}
	subsys_notif_register_notifier("adsp", &anb);

	audio_ocmem_lcl.ocmem_dump_addr =
		allocate_contiguous_memory_nomap(AUDIO_OCMEM_BUF_SIZE,
						 MEMTYPE_EBI1,
						 AUDIO_OCMEM_BUF_SIZE);

	if (audio_ocmem_lcl.ocmem_dump_addr) {
		audio_ocmem_lcl.ocmem_ramdump_dev =
			create_ramdump_device("audio-ocmem", &pdev->dev);

		if (!audio_ocmem_lcl.ocmem_ramdump_dev)
			pr_info("%s: audio-ocmem ramdump device failed\n",
				__func__);
	} else {
		pr_err("%s: ocmem dump memory alloc failed\n", __func__);
	}

	audio_ocmem_lcl.audio_ocmem_workqueue =
		alloc_workqueue("ocmem_audio_client_driver_audio",
					WQ_NON_REENTRANT | WQ_UNBOUND, 0);
	if (!audio_ocmem_lcl.audio_ocmem_workqueue) {
		pr_err("%s: Failed to create ocmem audio work queue\n",
			__func__);
		ret = -ENOMEM;
		goto destroy_ramdump;
	}

	audio_ocmem_lcl.voice_ocmem_workqueue =
		alloc_workqueue("ocmem_audio_client_driver_voice",
					WQ_NON_REENTRANT, 0);
	if (!audio_ocmem_lcl.voice_ocmem_workqueue) {
		pr_info("%s: Failed to create ocmem voice work queue\n",
			__func__);
		ret = -ENOMEM;
		goto destroy_audio_wq;
	}

	init_waitqueue_head(&audio_ocmem_lcl.audio_wait);
	atomic_set(&audio_ocmem_lcl.audio_cond, 1);
	atomic_set(&audio_ocmem_lcl.audio_state, OCMEM_STATE_DEFAULT);
	atomic_set(&audio_ocmem_lcl.audio_exit, 0);
	spin_lock_init(&audio_ocmem_lcl.audio_lock);
	mutex_init(&audio_ocmem_lcl.state_process_lock);
	audio_ocmem_lcl.ocmem_en = true;
	audio_ocmem_lcl.audio_ocmem_running = false;

	/* populate platform data */
	ret = audio_ocmem_platform_data_populate(pdev);
	if (ret) {
		dev_err(&pdev->dev, "%s: failed to populate platform data, rc = %d\n",
						__func__, ret);
		goto destroy_voice_wq;
	}
	audio_ocmem_bus_scale_pdata = dev_get_drvdata(&pdev->dev);

	audio_ocmem_lcl.audio_ocmem_bus_client =
		msm_bus_scale_register_client(audio_ocmem_bus_scale_pdata);

	if (!audio_ocmem_lcl.audio_ocmem_bus_client) {
		pr_err("%s: msm_bus_scale_register_client() failed\n",
		__func__);
		ret = -EFAULT;
		goto destroy_voice_wq;
	}
	audio_ocmem_lcl.lp_memseg_ptr = NULL;
	return 0;
destroy_voice_wq:
	if (audio_ocmem_lcl.voice_ocmem_workqueue) {
		destroy_workqueue(audio_ocmem_lcl.voice_ocmem_workqueue);
		audio_ocmem_lcl.voice_ocmem_workqueue = NULL;
	}
destroy_audio_wq:
	if (audio_ocmem_lcl.audio_ocmem_workqueue) {
		destroy_workqueue(audio_ocmem_lcl.audio_ocmem_workqueue);
		audio_ocmem_lcl.audio_ocmem_workqueue = NULL;
	}
destroy_ramdump:
	if (audio_ocmem_lcl.ocmem_ramdump_dev)
		destroy_ramdump_device(audio_ocmem_lcl.ocmem_ramdump_dev);
	if (audio_ocmem_lcl.ocmem_dump_addr)
		free_contiguous_memory_by_paddr(
		    audio_ocmem_lcl.ocmem_dump_addr);
	return ret;
}

static int ocmem_audio_client_remove(struct platform_device *pdev)
{
	struct msm_bus_scale_pdata *audio_ocmem_bus_scale_pdata = NULL;

	audio_ocmem_bus_scale_pdata = (struct msm_bus_scale_pdata *)
					dev_get_drvdata(&pdev->dev);

	msm_bus_cl_clear_pdata(audio_ocmem_bus_scale_pdata);
	ocmem_notifier_unregister(audio_ocmem_lcl.audio_hdl,
					&audio_ocmem_client_nb);
	if (audio_ocmem_lcl.ocmem_ramdump_dev)
		destroy_ramdump_device(audio_ocmem_lcl.ocmem_ramdump_dev);
	if (audio_ocmem_lcl.ocmem_dump_addr)
		free_contiguous_memory_by_paddr(
		    audio_ocmem_lcl.ocmem_dump_addr);

	return 0;
}
static const struct of_device_id msm_ocmem_audio_dt_match[] = {
	{.compatible = "qcom,msm-ocmem-audio"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_ocmem_audio_dt_match);

static struct platform_driver audio_ocmem_driver = {
	.driver = {
		.name = "audio-ocmem",
		.owner = THIS_MODULE,
		.of_match_table = msm_ocmem_audio_dt_match,
	},
	.probe = ocmem_audio_client_probe,
	.remove = ocmem_audio_client_remove,
};

static int __init ocmem_audio_client_init(void)
{
	int rc;
	rc = platform_driver_register(&audio_ocmem_driver);

	if (rc)
		pr_err("%s: Failed to register audio ocmem driver\n", __func__);
	return rc;
}
module_init(ocmem_audio_client_init);

static void __exit ocmem_audio_client_exit(void)
{
	platform_driver_unregister(&audio_ocmem_driver);
}

module_exit(ocmem_audio_client_exit);
