/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/of_device.h>
#include <asm/mach-types.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/ocmem.h>
#include "q6core.h"
#include "audio_ocmem.h"

#define AUDIO_OCMEM_BUF_SIZE (512 * SZ_1K)

enum {
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
	struct workqueue_struct *audio_ocmem_workqueue;
	struct workqueue_struct *voice_ocmem_workqueue;
};

static struct audio_ocmem_prv audio_ocmem_lcl;


static int audio_ocmem_client_cb(struct notifier_block *this,
		 unsigned long event1, void *data)
{
	int rc = NOTIFY_DONE;
	unsigned long flags;

	pr_debug("%s: event[%ld] cur state[%x]\n", __func__,
			event1, atomic_read(&audio_ocmem_lcl.audio_state));

	spin_lock_irqsave(&audio_ocmem_lcl.audio_lock, flags);
	switch (event1) {
	case OCMEM_MAP_DONE:
		pr_debug("%s: map done\n", __func__);
		atomic_set(&audio_ocmem_lcl.audio_state, OCMEM_STATE_MAP_COMPL);
		break;
	case OCMEM_MAP_FAIL:
		pr_debug("%s: map fail\n", __func__);
		atomic_set(&audio_ocmem_lcl.audio_state, OCMEM_STATE_MAP_FAIL);
		break;
	case OCMEM_UNMAP_DONE:
		pr_debug("%s: unmap done\n", __func__);
		atomic_set(&audio_ocmem_lcl.audio_state,
				OCMEM_STATE_UNMAP_COMPL);
		break;
	case OCMEM_UNMAP_FAIL:
		pr_debug("%s: unmap fail\n", __func__);
		atomic_set(&audio_ocmem_lcl.audio_state,
				OCMEM_STATE_UNMAP_FAIL);
		break;
	case OCMEM_ALLOC_GROW:
		audio_ocmem_lcl.buf = data;
		atomic_set(&audio_ocmem_lcl.audio_state, OCMEM_STATE_GROW);
		break;
	case OCMEM_ALLOC_SHRINK:
		atomic_set(&audio_ocmem_lcl.audio_state, OCMEM_STATE_SHRINK);
		break;
	default:
		pr_err("%s: Invalid event[%ld]\n", __func__, event1);
		break;
	}
	spin_unlock_irqrestore(&audio_ocmem_lcl.audio_lock, flags);
	if (atomic_read(&audio_ocmem_lcl.audio_cond)) {
		atomic_set(&audio_ocmem_lcl.audio_cond, 0);
		wake_up(&audio_ocmem_lcl.audio_wait);
	}
	return rc;
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
	struct ocmem_buf *buf = NULL;
	struct avcs_cmd_rsp_get_low_power_segments_info_t *lp_segptr;

	pr_debug("%s\n", __func__);
	/* Non-blocking ocmem allocate (asynchronous) */
	buf = ocmem_allocate_nb(cid, AUDIO_OCMEM_BUF_SIZE);
	if (IS_ERR_OR_NULL(buf)) {
		pr_err("%s: failed: %d\n", __func__, cid);
		return -ENOMEM;
	}
	atomic_set(&audio_ocmem_lcl.audio_state, OCMEM_STATE_ALLOC);

	audio_ocmem_lcl.buf = buf;
	atomic_set(&audio_ocmem_lcl.audio_exit, 0);
	if (!buf->len) {
		wait_event_interruptible(audio_ocmem_lcl.audio_wait,
			(atomic_read(&audio_ocmem_lcl.audio_cond) == 0)	||
			(atomic_read(&audio_ocmem_lcl.audio_exit) == 1));

		if (atomic_read(&audio_ocmem_lcl.audio_exit)) {
			pr_err("%s: audio playback ended while waiting for ocmem\n",
					__func__);
			ret = -EINVAL;
			goto fail_cmd;
		}
	}
	if (audio_ocmem_lcl.lp_memseg_ptr == NULL) {
		/* Retrieve low power segments */
		ret = core_get_low_power_segments(
					&audio_ocmem_lcl.lp_memseg_ptr);
		if (ret != 0) {
			pr_err("%s: get low power segments from DSP failed, rc=%d\n",
					__func__, ret);
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
		pr_debug("%s: ro:%d, ddr_paddr[%x], size[%x]\n", __func__,
			audio_ocmem_lcl.mlist.chunks[j].ro,
			(uint32_t)audio_ocmem_lcl.mlist.chunks[j].ddr_paddr,
			(uint32_t)audio_ocmem_lcl.mlist.chunks[j].size);
	}

	/* vote for ocmem bus bandwidth */
	ret = msm_bus_scale_client_update_request(
				audio_ocmem_lcl.audio_ocmem_bus_client,
				0);
	if (ret)
		pr_err("%s: failed to vote for bus bandwidth\n", __func__);

	atomic_set(&audio_ocmem_lcl.audio_state, OCMEM_STATE_MAP_TRANSITION);

	ret = ocmem_map(cid, audio_ocmem_lcl.buf, &audio_ocmem_lcl.mlist);
	if (ret) {
		pr_err("%s: ocmem_map failed\n", __func__);
		goto fail_cmd;
	}


	while ((atomic_read(&audio_ocmem_lcl.audio_state) !=
						OCMEM_STATE_EXIT)) {

		wait_event_interruptible(audio_ocmem_lcl.audio_wait,
				atomic_read(&audio_ocmem_lcl.audio_cond) == 0);

		switch (atomic_read(&audio_ocmem_lcl.audio_state)) {
		case OCMEM_STATE_MAP_COMPL:
			pr_debug("%s: audio_cond[0x%x], audio_state[0x%x]\n",
			__func__, atomic_read(&audio_ocmem_lcl.audio_cond),
			atomic_read(&audio_ocmem_lcl.audio_state));
			atomic_set(&audio_ocmem_lcl.audio_state,
					OCMEM_STATE_MAP_COMPL);
			atomic_set(&audio_ocmem_lcl.audio_cond, 1);
			break;
		case OCMEM_STATE_SHRINK:
			atomic_set(&audio_ocmem_lcl.audio_cond, 1);
			ret = ocmem_unmap(cid, audio_ocmem_lcl.buf,
					&audio_ocmem_lcl.mlist);
			if (ret) {
				pr_err("%s: ocmem_unmap failed, state[%d]\n",
				__func__,
				atomic_read(&audio_ocmem_lcl.audio_state));
				goto fail_cmd;
			}

			atomic_set(&audio_ocmem_lcl.audio_state,
					OCMEM_STATE_UNMAP_TRANSITION);
			wait_event_interruptible(audio_ocmem_lcl.audio_wait,
				atomic_read(&audio_ocmem_lcl.audio_cond) == 0);
			atomic_set(&audio_ocmem_lcl.audio_state,
					OCMEM_STATE_UNMAP_COMPL);
			ret = ocmem_shrink(cid, audio_ocmem_lcl.buf, 0);
			if (ret) {
				pr_err("%s: ocmem_shrink failed, state[%d]\n",
				__func__,
				atomic_read(&audio_ocmem_lcl.audio_state));
				goto fail_cmd;
			}

			break;
		case OCMEM_STATE_GROW:
			atomic_set(&audio_ocmem_lcl.audio_cond, 1);
			ret = ocmem_map(cid, audio_ocmem_lcl.buf,
						&audio_ocmem_lcl.mlist);
			if (ret) {
				pr_err("%s: ocmem_map failed, state[%d]\n",
				__func__,
				atomic_read(&audio_ocmem_lcl.audio_state));
				goto fail_cmd;
			}
			atomic_set(&audio_ocmem_lcl.audio_state,
				OCMEM_STATE_MAP_TRANSITION);
			wait_event_interruptible(audio_ocmem_lcl.audio_wait,
				atomic_read(&audio_ocmem_lcl.audio_cond) == 0);
			atomic_set(&audio_ocmem_lcl.audio_state,
				OCMEM_STATE_MAP_COMPL);
			break;
		}
	}
fail_cmd:
	pr_debug("%s: exit\n", __func__);
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
	int ret;

	if (atomic_read(&audio_ocmem_lcl.audio_cond))
		atomic_set(&audio_ocmem_lcl.audio_cond, 0);
	pr_debug("%s: audio_cond[0x%x], audio_state[0x%x]\n", __func__,
			 atomic_read(&audio_ocmem_lcl.audio_cond),
			 atomic_read(&audio_ocmem_lcl.audio_state));
	switch (atomic_read(&audio_ocmem_lcl.audio_state)) {
	case OCMEM_STATE_MAP_COMPL:
		atomic_set(&audio_ocmem_lcl.audio_cond, 1);
		ret = ocmem_unmap(cid, audio_ocmem_lcl.buf,
					&audio_ocmem_lcl.mlist);
		if (ret) {
			pr_err("%s: ocmem_unmap failed, state[%d]\n",
				__func__,
				atomic_read(&audio_ocmem_lcl.audio_state));
			goto fail_cmd;
		}

		atomic_set(&audio_ocmem_lcl.audio_state, OCMEM_STATE_EXIT);

		wait_event_interruptible(audio_ocmem_lcl.audio_wait,
				atomic_read(&audio_ocmem_lcl.audio_cond) == 0);
	case OCMEM_STATE_UNMAP_COMPL:
		ret = ocmem_free(OCMEM_LP_AUDIO, audio_ocmem_lcl.buf);
		if (ret) {
			pr_err("%s: ocmem_free failed, state[%d]\n",
				__func__,
				atomic_read(&audio_ocmem_lcl.audio_state));
			goto fail_cmd;
		}
		pr_debug("%s: ocmem_free success\n", __func__);
	default:
		pr_debug("%s: state=%d", __func__,
			atomic_read(&audio_ocmem_lcl.audio_state));
		break;

	}
	return 0;
fail_cmd:
	return ret;
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

	if (audio_ocmem_lcl.voice_ocmem_workqueue == NULL) {
		pr_err("%s: voice ocmem workqueue is NULL\n", __func__);
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
	queue_work(audio_ocmem_lcl.voice_ocmem_workqueue, &workdata->work);

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

	if (audio_ocmem_lcl.audio_ocmem_workqueue == NULL) {
		pr_err("%s: audio ocmem workqueue is NULL\n", __func__);
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

	/* if previous work waiting for ocmem - signal it to exit */
	atomic_set(&audio_ocmem_lcl.audio_exit, 1);

	INIT_WORK(&workdata->work, audio_ocmem_process_workdata);
	queue_work(audio_ocmem_lcl.audio_ocmem_workqueue, &workdata->work);

	return 0;
}


static struct notifier_block audio_ocmem_client_nb = {
	.notifier_call = audio_ocmem_client_cb,
};

static int audio_ocmem_platform_data_populate(struct platform_device *pdev)
{
	int ret;
	struct msm_bus_scale_pdata *audio_ocmem_bus_scale_pdata = NULL;
	struct msm_bus_vectors *audio_ocmem_bus_vectors = NULL;
	struct msm_bus_paths *ocmem_audio_bus_paths = NULL;
	u32 val;

	if (!pdev->dev.of_node) {
		pr_err("%s: device tree information missing\n", __func__);
		return -ENODEV;
	}

	audio_ocmem_bus_vectors = kzalloc(sizeof(struct msm_bus_vectors),
								GFP_KERNEL);
	if (!audio_ocmem_bus_vectors) {
		dev_err(&pdev->dev, "Failed to allocate memory for platform data\n");
		return -ENOMEM;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				"qcom,msm-ocmem-audio-src-id", &val);
	if (ret) {
		dev_err(&pdev->dev, "%s: qcom,msm-ocmem-audio-src-id missing in DT node\n",
				__func__);
		goto fail1;
	}
	audio_ocmem_bus_vectors->src = val;
	ret = of_property_read_u32(pdev->dev.of_node,
				"qcom,msm-ocmem-audio-dst-id", &val);
	if (ret) {
		dev_err(&pdev->dev, "%s: qcom,msm-ocmem-audio-dst-id missing in DT node\n",
				__func__);
		goto fail1;
	}
	audio_ocmem_bus_vectors->dst = val;
	ret = of_property_read_u32(pdev->dev.of_node,
				"qcom,msm-ocmem-audio-ab", &val);
	if (ret) {
		dev_err(&pdev->dev, "%s: qcom,msm-ocmem-audio-ab missing in DT node\n",
					__func__);
		goto fail1;
	}
	audio_ocmem_bus_vectors->ab = val;
	ret = of_property_read_u32(pdev->dev.of_node,
				"qcom,msm-ocmem-audio-ib", &val);
	if (ret) {
		dev_err(&pdev->dev, "%s: qcom,msm-ocmem-audio-ib missing in DT node\n",
					__func__);
		goto fail1;
	}
	audio_ocmem_bus_vectors->ib = val;

	ocmem_audio_bus_paths = kzalloc(sizeof(struct msm_bus_paths),
								GFP_KERNEL);
	if (!ocmem_audio_bus_paths) {
		dev_err(&pdev->dev, "Failed to allocate memory for platform data\n");
		goto fail1;
	}
	ocmem_audio_bus_paths->num_paths = 1;
	ocmem_audio_bus_paths->vectors = audio_ocmem_bus_vectors;

	audio_ocmem_bus_scale_pdata =
		kzalloc(sizeof(struct msm_bus_scale_pdata), GFP_KERNEL);

	if (!audio_ocmem_bus_scale_pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory for platform data\n");
		goto fail2;
	}

	audio_ocmem_bus_scale_pdata->usecase = ocmem_audio_bus_paths;
	audio_ocmem_bus_scale_pdata->num_usecases = 1;
	audio_ocmem_bus_scale_pdata->name = "audio-ocmem";

	dev_set_drvdata(&pdev->dev, audio_ocmem_bus_scale_pdata);
	return ret;

fail2:
	kfree(ocmem_audio_bus_paths);
fail1:
	kfree(audio_ocmem_bus_vectors);
	return ret;
}
static int ocmem_audio_client_probe(struct platform_device *pdev)
{
	int ret;
	struct msm_bus_scale_pdata *audio_ocmem_bus_scale_pdata = NULL;

	pr_debug("%s\n", __func__);
	audio_ocmem_lcl.audio_ocmem_workqueue =
		alloc_workqueue("ocmem_audio_client_driver_audio",
					WQ_NON_REENTRANT, 0);
	if (!audio_ocmem_lcl.audio_ocmem_workqueue) {
		pr_err("%s: Failed to create ocmem audio work queue\n",
			__func__);
		return -ENOMEM;
	}

	audio_ocmem_lcl.voice_ocmem_workqueue =
		alloc_workqueue("ocmem_audio_client_driver_voice",
					WQ_NON_REENTRANT, 0);
	if (!audio_ocmem_lcl.voice_ocmem_workqueue) {
		pr_err("%s: Failed to create ocmem voice work queue\n",
			__func__);
		return -ENOMEM;
	}

	init_waitqueue_head(&audio_ocmem_lcl.audio_wait);
	atomic_set(&audio_ocmem_lcl.audio_cond, 1);
	atomic_set(&audio_ocmem_lcl.audio_exit, 0);
	spin_lock_init(&audio_ocmem_lcl.audio_lock);

	/* populate platform data */
	ret = audio_ocmem_platform_data_populate(pdev);
	if (ret) {
		dev_err(&pdev->dev, "%s: failed to populate platform data, rc = %d\n",
						__func__, ret);
		return -ENODEV;
	}
	audio_ocmem_bus_scale_pdata = dev_get_drvdata(&pdev->dev);

	audio_ocmem_lcl.audio_ocmem_bus_client =
		msm_bus_scale_register_client(audio_ocmem_bus_scale_pdata);

	if (!audio_ocmem_lcl.audio_ocmem_bus_client) {
		pr_err("%s: msm_bus_scale_register_client() failed\n",
		__func__);
		return -EFAULT;
	}
	audio_ocmem_lcl.audio_hdl = ocmem_notifier_register(OCMEM_LP_AUDIO,
						&audio_ocmem_client_nb);
	if (audio_ocmem_lcl.audio_hdl == NULL) {
		pr_err("%s: Failed to get ocmem handle %d\n", __func__,
						OCMEM_LP_AUDIO);
	}
	audio_ocmem_lcl.lp_memseg_ptr = NULL;
	return 0;
}

static int ocmem_audio_client_remove(struct platform_device *pdev)
{
	struct msm_bus_scale_pdata *audio_ocmem_bus_scale_pdata = NULL;

	audio_ocmem_bus_scale_pdata = (struct msm_bus_scale_pdata *)
					dev_get_drvdata(&pdev->dev);

	kfree(audio_ocmem_bus_scale_pdata->usecase->vectors);
	kfree(audio_ocmem_bus_scale_pdata->usecase);
	kfree(audio_ocmem_bus_scale_pdata);
	ocmem_notifier_unregister(audio_ocmem_lcl.audio_hdl,
					&audio_ocmem_client_nb);
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
