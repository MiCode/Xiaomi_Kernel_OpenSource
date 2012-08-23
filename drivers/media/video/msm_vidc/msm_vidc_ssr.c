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
 *
 */
#include <msm_vidc_ssr.h>

static struct msm_vidc_core *get_vidc_core_from_dev(struct device *dev)
{
	struct video_device *vdev;
	struct msm_video_device *videodev;
	struct msm_vidc_core *core;
	vdev = container_of(dev, struct video_device, dev);
	videodev = container_of(vdev, struct msm_video_device, vdev);
	core = container_of(videodev, struct msm_vidc_core,
		vdev[MSM_VIDC_DECODER]);
	return core;
}
int msm_vidc_shutdown(const struct subsys_desc *subsys)
{
	struct msm_vidc_inst *inst;
	struct msm_vidc_core *core = NULL;
	struct v4l2_event dqevent;
	struct device *dev;
	unsigned long flags;
	int rc = 0;
	if (!subsys) {
		dprintk(VIDC_ERR, "Invalid subsys: %p\n", subsys);
		rc = -EINVAL;
		goto exit;
	}
	dev = subsys->dev;
	if (dev)
		core = get_vidc_core_from_dev(dev);
	if (!core) {
		dprintk(VIDC_ERR, "Invalid core: %p\n", core);
		rc = -EINVAL;
		goto exit;
	}
	core->ssr_info.ssr_in_progress = true;
	spin_lock_irqsave(&core->lock, flags);
	core->state = VIDC_CORE_INVALID;
	spin_unlock_irqrestore(&core->lock, flags);
	dqevent.type = V4L2_EVENT_MSM_VIDC_SYS_ERROR;
	dqevent.id = 0;
	list_for_each_entry(inst, &core->instances, list) {
		if (inst) {
			v4l2_event_queue_fh(&inst->event_handler, &dqevent);
			spin_lock_irqsave(&inst->lock, flags);
			inst->state = MSM_VIDC_CORE_INVALID;
			spin_unlock_irqrestore(&inst->lock, flags);
		}
	}
exit:
	return rc;
}
int msm_vidc_ramdump(int enable, const struct subsys_desc *subsys)
{
	struct ramdump_segment memory_segments[] = {{0x0f500000, 0xFF000} };
	struct msm_vidc_core *core = NULL;
	void *dump_addr = NULL;
	int rc = 0;
	struct device *dev;
	if (!subsys) {
		dprintk(VIDC_ERR, "Invalid subsys: %p\n", subsys);
		rc = -EINVAL;
		goto exit;
	}
	dev = subsys->dev;
	if (dev)
		core = get_vidc_core_from_dev(dev);
	if (!core) {
		dprintk(VIDC_ERR, "Invalid core: %p\n", core);
		rc = -EINVAL;
		goto exit;
	}
	if (enable) {
		rc = do_ramdump(core->ssr_info.msm_vidc_ramdump_dev,
			memory_segments,
			ARRAY_SIZE(memory_segments));
		if (rc < 0)
			dprintk(VIDC_DBG, "Failed : FW image memory dump\n");
		dump_addr = kzalloc(core->resources.ocmem.buf->len, GFP_KERNEL);
		if (dump_addr)
			rc = ocmem_dump(OCMEM_VIDEO, core->resources.ocmem.buf,
				(unsigned long)dump_addr);
		if (rc < 0) {
			dprintk(VIDC_DBG, "Failed : OCMEM copy\n");
		} else	{
			memory_segments[0].address = (unsigned long)dump_addr;
			memory_segments[0].size =
				(unsigned long)core->resources.ocmem.buf->len;
			rc = do_ramdump(core->ssr_info.msm_vidc_ramdump_dev,
				memory_segments,
				ARRAY_SIZE(memory_segments));
			if (rc < 0)
				dprintk(VIDC_DBG, "Failed : OCMEM dump\n");
		}
		kfree(dump_addr);
	}
exit:
	return rc;
}
int msm_vidc_powerup(const struct subsys_desc *subsys)
{
	unsigned long flags;
	struct msm_vidc_core *core = NULL;
	int rc = 0;
	struct device *dev;
	if (!subsys) {
		dprintk(VIDC_ERR, "Invalid subsys: %p\n", subsys);
		rc = -EINVAL;
		goto exit;
	}
	dev = subsys->dev;
	if (dev)
		core = get_vidc_core_from_dev(dev);
	if (!core) {
		dprintk(VIDC_ERR, "Invalid core: %p\n", core);
		rc = -EINVAL;
		goto exit;
	}
	msm_comm_free_ocmem(core);
	vidc_hal_core_release(core->device);
	spin_lock_irqsave(&core->lock, flags);
	core->state = VIDC_CORE_UNINIT;
	spin_unlock_irqrestore(&core->lock, flags);
	msm_comm_unload_fw(core);
exit:
	return rc;
}
void msm_vidc_crash_shutdown(const struct subsys_desc *subsys)
{
	dprintk(VIDC_DBG, "Nothing implemented in crash shutdown\n");
}
static struct subsys_desc msm_vidc_subsystem = {
	.name = "msm_vidc",
	.dev = NULL,
	.shutdown = msm_vidc_shutdown,
	.powerup = msm_vidc_powerup,
	.ramdump = msm_vidc_ramdump,
	.crash_shutdown = msm_vidc_crash_shutdown
};
int msm_vidc_ssr_init(struct msm_vidc_core *core)
{
	int rc = 0;
	msm_vidc_subsystem.dev = &core->vdev[MSM_VIDC_DECODER].vdev.dev;
	core->ssr_info.msm_vidc_dev = subsys_register(&msm_vidc_subsystem);
	if (IS_ERR_OR_NULL(core->ssr_info.msm_vidc_dev)) {
		dprintk(VIDC_ERR, "msm_vidc Sub System registration failed\n");
		rc = -ENODEV;
	}
	core->ssr_info.msm_vidc_ramdump_dev = create_ramdump_device("msm_vidc");
	if (!core->ssr_info.msm_vidc_ramdump_dev) {
		dprintk(VIDC_ERR, "Unable to create msm_vidc ramdump device\n");
		rc = -ENODEV;
	}
	core->ssr_info.ssr_in_progress = false;
	return rc;
}

int msm_vidc_ssr_uninit(struct msm_vidc_core *core)
{
	subsys_unregister(core->ssr_info.msm_vidc_dev);
	destroy_ramdump_device(core->ssr_info.msm_vidc_ramdump_dev);
	return 0;
}
