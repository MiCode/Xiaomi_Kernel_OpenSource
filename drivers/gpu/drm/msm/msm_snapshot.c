/* Copyright (c) 2016 The Linux Foundation. All rights reserved.
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

#include "msm_gpu.h"
#include "msm_gem.h"
#include "msm_snapshot_api.h"

void msm_snapshot_destroy(struct msm_gpu *gpu, struct msm_snapshot *snapshot)
{
	struct drm_device *dev = gpu->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct platform_device *pdev = priv->gpu_pdev;

	if (!snapshot)
		return;

	dma_free_coherent(&pdev->dev, SZ_1M, snapshot->ptr,
		snapshot->physaddr);

	kfree(snapshot);
}

struct msm_snapshot *msm_snapshot_new(struct msm_gpu *gpu)
{
	struct drm_device *dev = gpu->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct platform_device *pdev = priv->gpu_pdev;
	struct msm_snapshot *snapshot;

	snapshot = kzalloc(sizeof(*snapshot), GFP_KERNEL);
	if (!snapshot)
		return ERR_PTR(-ENOMEM);

	snapshot->ptr = dma_alloc_coherent(&pdev->dev, SZ_1M,
		&snapshot->physaddr, GFP_KERNEL);

	if (!snapshot->ptr) {
		kfree(snapshot);
		return ERR_PTR(-ENOMEM);
	}

	seq_buf_init(&snapshot->buf, snapshot->ptr, SZ_1M);

	return snapshot;
}

int msm_gpu_snapshot(struct msm_gpu *gpu, struct msm_snapshot *snapshot)
{
	int ret;
	struct msm_snapshot_header header;
	uint64_t val;

	if (!snapshot)
		return -ENOMEM;

	/*
	 * For now, blow away the snapshot and take a new one  - the most
	 * interesting hang is the last one we saw
	 */
	seq_buf_init(&snapshot->buf, snapshot->ptr, SZ_1M);

	header.magic = SNAPSHOT_MAGIC;
	gpu->funcs->get_param(gpu, MSM_PARAM_GPU_ID, &val);
	header.gpuid = lower_32_bits(val);

	gpu->funcs->get_param(gpu, MSM_PARAM_CHIP_ID, &val);
	header.chipid = lower_32_bits(val);

	seq_buf_putmem(&snapshot->buf, &header, sizeof(header));

	ret = gpu->funcs->snapshot(gpu, snapshot);

	if (!ret) {
		struct msm_snapshot_section_header end;

		end.magic = SNAPSHOT_SECTION_MAGIC;
		end.id = SNAPSHOT_SECTION_END;
		end.size = sizeof(end);

		seq_buf_putmem(&snapshot->buf, &end, sizeof(end));

		dev_info(gpu->dev->dev, "GPU snapshot created [0x%pa (%d bytes)]\n",
			&snapshot->physaddr, seq_buf_used(&snapshot->buf));
	}

	return ret;
}

int msm_snapshot_write(struct msm_gpu *gpu, struct seq_file *m)
{
	if (gpu && gpu->snapshot)
		seq_write(m, gpu->snapshot->ptr,
			seq_buf_used(&gpu->snapshot->buf));

	return 0;
}
