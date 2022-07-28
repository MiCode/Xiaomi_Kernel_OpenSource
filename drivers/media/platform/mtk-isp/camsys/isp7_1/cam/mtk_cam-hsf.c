// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Louis Kuo <louis.kuo@mediatek.com>
 */
#include <linux/component.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>

#include <linux/platform_data/mtk_ccd.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-subdev.h>
#include "mtk_cam.h"
#include "mtk_heap.h"
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/arm-smccc.h>

static struct device *mtk_cam_find_raw_dev_hsf(struct mtk_cam_device *cam,
					   unsigned int raw_mask)
{
	struct mtk_cam_ctx *ctx;
	unsigned int i;

	for (i = 0; i < cam->num_raw_drivers; i++) {
		if (raw_mask & (1 << i)) {
			ctx = cam->ctxs + i;
			/* FIXME: correct TWIN case */
			return cam->raw.devs[i];
		}
	}
	return NULL;
}

struct dma_buf *mtk_cam_dmabuf_alloc(struct mtk_cam_ctx *ctx, unsigned int size)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct dma_heap *heap_type;
	struct dma_buf *dbuf;

	heap_type = dma_heap_find("mtk_prot_region-aligned");

	if (!heap_type) {
		dev_info(cam->dev, "heap type error\n");
		return NULL;
	}

	dbuf = dma_heap_buffer_alloc(heap_type, size,
		O_CLOEXEC | O_RDWR, DMA_HEAP_VALID_HEAP_FLAGS);

	dma_heap_put(heap_type);
	if (IS_ERR(dbuf)) {
		dev_info(cam->dev, "region-based hsf buffer allocation fail\n");
		return NULL;
	}
	dev_info(cam->dev, "%s done dbuf = 0x%x\n", __func__, dbuf);
	return dbuf;
}

int mtk_cam_dmabuf_get_iova(struct mtk_cam_ctx *ctx,
	struct device *dev, struct mtk_cam_dma_map *dmap)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	int handle;

	handle = dmabuf_to_secure_handle(dmap->dbuf);

	dev_info(cam->dev, "get dbuf dmabuf_to_secure_handle done\n");
	if (!handle) {
		dev_info(cam->dev, "get hsf handle failed\n");
		return -1;
	}

	attach = dma_buf_attach(dmap->dbuf, dev);
		dev_info(cam->dev, "get dbuf dma_buf_attach done\n");
	if (IS_ERR(attach)) {
		dev_info(cam->dev, "dma_buf_attach failed\n");
		return -1;
	}

	table = dma_buf_map_attachment(attach, DMA_TO_DEVICE);
	dev_info(cam->dev, "get dbuf dma_buf_map_attachment done\n");

	if (IS_ERR(table)) {
		dev_info(cam->dev, "dma_buf_map_attachment failed\n");
		dma_buf_detach(dmap->dbuf, attach);
		return -1;
	}

	dmap->hsf_handle = handle;
	dmap->attach = attach;
	dmap->table = table;
	dmap->dma_addr = sg_dma_address(table->sgl);

	return 0;
}

void mtk_cam_dmabuf_free_iova(struct mtk_cam_ctx *ctx, struct mtk_cam_dma_map *dmap)
{
	if (dmap->attach == NULL || dmap->table == NULL) {
		//dev_info(cam->dev, "dmap is null\n");
		return;
	}
	dma_buf_unmap_attachment(dmap->attach, dmap->table, DMA_TO_DEVICE);
	dma_buf_detach(dmap->dbuf, dmap->attach);
	dma_heap_buffer_free(dmap->dbuf);
}

#ifdef USING_CCU
static unsigned int get_ccu_device(struct mtk_cam_hsf_ctrl *handle_inst)
{
	int ret = 0;
	phandle handle;
	struct device_node *node = NULL, *rproc_np = NULL;

    /* clear data */
	handle_inst->ccu_pdev = NULL;
	handle_inst->ccu_handle = 0;
	node = of_find_compatible_node(NULL, NULL, "mediatek,camera_camsys_ccu");
	if (!node) {
		pr_info("error: find mediatek,camera_camsys_ccu failed!!!\n");
		return 1;
	}
	ret = of_property_read_u32(node, "mediatek,ccu_rproc", &handle);
	if (ret < 0) {
		pr_info("error: ccu_rproc of_property_read_u32:%d\n", ret);
		return ret;
	}
	rproc_np = of_find_node_by_phandle(handle);
	if (rproc_np) {
		handle_inst->ccu_pdev = of_find_device_by_node(rproc_np);
		pr_info("handle_inst.ccu_pdev = 0x%x\n", handle_inst->ccu_pdev);
		if (!handle_inst->ccu_pdev) {
			pr_info("error: failed to find ccu rproc pdev\n");
			handle_inst->ccu_pdev = NULL;
			return ret;
		}
		/* keep for rproc_get_by_phandle() using */
		handle_inst->ccu_handle = handle;
		pr_info("get ccu proc pdev successfully\n");
	}
	return ret;
}

static void mtk_cam_power_on_ccu(struct mtk_cam_hsf_ctrl *handle_inst, unsigned int flag)
{
	int ret = 0;
	struct rproc *ccu_rproc = NULL;

	if (!handle_inst->ccu_pdev)
		get_ccu_device(handle_inst);

	ccu_rproc = rproc_get_by_phandle(handle_inst->ccu_handle);
	if (ccu_rproc == NULL) {
		pr_info("error: ccu_rproc is null!\n");
		return;
	}
	if (flag > 0) {
	/* boot up ccu */
		ret = rproc_boot(ccu_rproc);
		if (ret != 0) {
			pr_info("error: ccu rproc_boot failed!\n");
			return;
		}

		handle_inst->power_on_cnt++;
		pr_info("camsys power on ccu, cnt:%d\n", handle_inst->power_on_cnt);
	} else {
		/* shutdown ccu */
		rproc_shutdown(ccu_rproc);
		handle_inst->power_on_cnt--;
		pr_info("camsys power off ccu, cnt:%d\n", handle_inst->power_on_cnt);
	}
}
#endif

void ccu_stream_on(struct mtk_raw_device *dev)
{
	int ret = 0;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_hsf_ctrl *hsf_config = NULL;
	struct mtk_cam_hsf_info *share_buf = NULL;
	struct raw_info pData;
#ifdef PERFORMANCE_HSF
	int ms_0 = 0, ms_1 = 0, ms = 0;
	struct timeval time;

	do_gettimeofday(&time);
	ms_0 = time.tv_sec + time.tv_usec;
#endif
	pData.tg_idx = dev->id;
	ctx = mtk_cam_find_ctx(dev->cam, &dev->pipeline->subdev.entity);

	if (ctx == NULL) {
		pr_info("%s error: ctx is NULL pointer check initial\n", __func__);
		return;
	}
	hsf_config = ctx->hsf;

	if (hsf_config == NULL) {
		pr_info("%s error: hsf_config is NULL pointer check hsf initial\n", __func__);
		return;
	}
	share_buf = hsf_config->share_buf;

	if (share_buf == NULL) {
		pr_info("%s hsf config fail share buffer not alloc\n", __func__);
		return;
	}
#ifdef USING_CCU
	ret = mtk_ccu_rproc_ipc_send(
	hsf_config->ccu_pdev,
	MTK_CCU_FEATURE_CAMSYS,
	MSG_TO_CCU_STREAM_ON,
	(void *)&pData, sizeof(struct raw_info));
#endif

#ifdef PERFORMANCE_HSF
	do_gettimeofday(&time);
	ms_1 = time.tv_sec + time.tv_usec;
	ms = ms_1 - ms_0;
	pr_info("%s time %d us\n", __func__, ms);
#endif

	if (ret != 0)
		pr_info("error: %s fail%d\n", __func__, pData.tg_idx);
	else
		pr_info("%s success\n", __func__, pData.tg_idx);
}

void ccu_hsf_config(struct mtk_cam_ctx *ctx, unsigned int En)
{
	int ret = 0;
	struct raw_info pData;
	struct mtk_cam_hsf_ctrl *hsf_config = NULL;
	struct mtk_cam_hsf_info *share_buf = NULL;
#ifdef PERFORMANCE_HSF
	int ms_0 = 0, ms_1 = 0, ms = 0;
	struct timeval time;

	do_gettimeofday(&time);
	ms_0 = time.tv_sec + time.tv_usec;
#endif
	if (ctx == NULL) {
		pr_info("%s error: ctx is NULL pointer check initial\n", __func__);
		return;
	}
	hsf_config = ctx->hsf;

	if (hsf_config == NULL) {
		pr_info("%s error: hsf_config is NULL pointer check hsf initial\n", __func__);
		return;
	}

	share_buf = hsf_config->share_buf;
	pData.tg_idx = share_buf->cam_tg;
	pData.chunk_iova = share_buf->chunk_iova;
	pData.cq_iova = share_buf->cq_dst_iova;
	pData.enable_raw = share_buf->enable_raw;
	pData.Hsf_en = En;

	pr_info("tg = %d  chunk_iova = 0x%lx  pData.Hsf_en = 0x%x\n",
		pData.tg_idx, pData.chunk_iova, pData.Hsf_en);
	ret = mtk_ccu_rproc_ipc_send(
		hsf_config->ccu_pdev,
		MTK_CCU_FEATURE_CAMSYS,
		MSG_TO_CCU_HSF_CONFIG,
		(void *)&pData, sizeof(struct raw_info));

#ifdef PERFORMANCE_HSF
	do_gettimeofday(&time);
	ms_1 = time.tv_sec + time.tv_usec;
	ms = ms_1 - ms_0;
	pr_info("%s time %d us\n", __func__, ms);
#endif

	if (ret != 0)
		pr_info("error: %s fail%d\n", __func__, pData.tg_idx);
	else
		pr_info("%s success\n", __func__, pData.tg_idx);
}



void ccu_apply_cq(struct mtk_raw_device *dev, dma_addr_t cq_addr,
	unsigned int cq_size, int initial, unsigned int cq_offset,
	unsigned int sub_cq_size, unsigned int sub_cq_offset)
{
	int ret = 0;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_hsf_ctrl *hsf_config = NULL;
	struct mtk_cam_hsf_info *share_buf = NULL;
	struct cq_info pData;
#ifdef PERFORMANCE_HSF
	int ms_0 = 0, ms_1 = 0, ms = 0;
	struct timeval time;

	do_gettimeofday(&time);
	ms_0 = time.tv_sec + time.tv_usec;
#endif

	ctx = mtk_cam_find_ctx(dev->cam, &dev->pipeline->subdev.entity);

	if (ctx == NULL) {
		pr_info("%s error: ctx is NULL pointer check initial\n", __func__);
		return;
	}
	hsf_config = ctx->hsf;

	if (hsf_config == NULL) {
		pr_info("%s error: hsf_config is NULL pointer check hsf initial\n", __func__);
		return;
	}
	share_buf = hsf_config->share_buf;

	if (share_buf == NULL) {
		pr_info("%s hsf config fail share buffer not alloc\n", __func__);
		return;
	}
#ifdef USING_CCU
	if (!(hsf_config->ccu_pdev))
		get_ccu_device(hsf_config);
#endif

    /* call CCU to trigger CQ*/
	pData.tg = share_buf->cam_tg;
	pData.dst_addr = share_buf->cq_dst_iova;
	pData.chunk_iova = share_buf->chunk_iova;
	pData.src_addr = cq_addr;
	pData.cq_size = cq_size;
	pData.init_value = initial;
	pData.cq_offset = cq_offset;
	pData.sub_cq_size = sub_cq_size;
	pData.sub_cq_offset = sub_cq_offset;
	pData.ipc_status = 0;

	pr_info("CCU trigger CQ. tg:%d cq_src:0x%lx cq_dst:0x%lx cq_addr = 0x%lx init_value = %d\n",
		pData.tg, pData.src_addr, pData.dst_addr, cq_addr, pData.init_value);
#ifdef USING_CCU
	pr_info("ccu_pdev = 0x%x\n", hsf_config->ccu_pdev);
	ret = mtk_ccu_rproc_ipc_send(
		hsf_config->ccu_pdev,
		MTK_CCU_FEATURE_CAMSYS,
		MSG_TO_CCU_HSF_APPLY_CQ,
		(void *)&pData, sizeof(struct cq_info));
#endif

#ifdef PERFORMANCE_HSF
	do_gettimeofday(&time);
	ms_1 = time.tv_sec + time.tv_usec;
	ms = ms_1 - ms_0;
	pr_info("%s time %d us\n", __func__, ms);
#endif

	if (ret != 0)
		pr_info("error: CCU trigger CQ failure. tg:%d cq_src:0x%x cq_dst:0x%x cq_size:%d\n",
		pData.tg, pData.src_addr, pData.dst_addr, pData.cq_size);
	else
		pr_info("after CCU trigger CQ. tg:%d cq_src:0x%x cq_dst:0x%x initial_value = %d\n",
		pData.tg, pData.src_addr, pData.dst_addr, pData.init_value);
	if (pData.ipc_status != 0)
		pr_info("error:CCU trrigger CQ Sensor initial fail 0x%x\n",
		pData.init_value);

}
int mtk_cam_hsf_init(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_hsf_ctrl *hsf_config;
	int Buf_s = 0;
#ifdef PERFORMANCE_HSF
	int ms_0 = 0, ms_1 = 0, ms = 0;
	struct timeval time;

	do_gettimeofday(&time);
	ms_0 = time.tv_sec + time.tv_usec;
#endif

	hsf_config = ctx->hsf = kmalloc(sizeof(struct mtk_cam_hsf_info), GFP_KERNEL);
	if (ctx->hsf == NULL) {
		dev_info(cam->dev, "ctx->hsf == NULL\n");
		return -1;
	}

	Buf_s = sizeof(struct mtk_cam_hsf_info);
	Buf_s = ((Buf_s) + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1); //align Page size
	hsf_config->share_buf = kmalloc(Buf_s, GFP_KERNEL);
	if (hsf_config->share_buf == NULL) {
		dev_info(cam->dev, "share_buf kmalloc fail\n");
		return -1;
	}
	memset(hsf_config->share_buf, 0, Buf_s); /*init data */

	hsf_config->cq_buf = kmalloc(sizeof(struct mtk_cam_dma_map), GFP_KERNEL);
	if (hsf_config->cq_buf == NULL)
		return -1;

	hsf_config->chk_buf = kmalloc(sizeof(struct mtk_cam_dma_map), GFP_KERNEL);
	if (hsf_config->chk_buf == NULL)
		return -1;

#ifdef PERFORMANCE_HSF
	do_gettimeofday(&time);
	ms_1 = time.tv_sec + time.tv_usec;
	ms = ms_1 - ms_0;
	dev_info(cam->dev, "hsf alloc time %d us\n", ms);
#endif

#ifdef USING_CCU
#ifdef PERFORMANCE_HSF
	do_gettimeofday(&time);
	ms_0 = time.tv_sec + time.tv_usec;
#endif
	get_ccu_device(hsf_config);
	mtk_cam_power_on_ccu(hsf_config, 1);
#ifdef PERFORMANCE_HSF
	do_gettimeofday(&time);
	ms_1 = time.tv_sec + time.tv_usec;
	ms = ms_1 - ms_0;
	dev_info(cam->dev, "ccu power on %d us\n", ms);
#endif
#endif
	dev_info(cam->dev, "hsf initial ready\n");
	return 0;
}

int mtk_cam_hsf_config(struct mtk_cam_ctx *ctx, unsigned int raw_id)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_raw_pipeline *pipe = ctx->pipe;
	struct mtk_cam_hsf_ctrl *hsf_config = NULL;
	struct mtk_cam_hsf_info *share_buf = NULL;
	struct mtk_cam_dma_map *dma_map_cq = NULL;
	struct mtk_cam_dma_map *dma_map_chk = NULL;
	struct device *dev;
	struct mtk_raw_device *raw_dev;
	struct arm_smccc_res res;

#ifdef PERFORMANCE_HSF
	int ms_0 = 0, ms_1 = 0, ms = 0;
	struct timeval time;

	do_gettimeofday(&time);
	ms_0 = time.tv_sec + time.tv_usec;
#endif
	if (mtk_cam_hsf_init(ctx) != 0) {
		dev_info(cam->dev, "hsf initial fail\n");
		return -1;
	}
	hsf_config = ctx->hsf;

	if (hsf_config == NULL) {
		pr_info("%s error: hsf_config is NULL pointer check hsf initial\n", __func__);
		return -1;
	}
	share_buf = hsf_config->share_buf;

	if (share_buf == NULL) {
		pr_info("%s hsf config fail share buffer not alloc\n", __func__);
		return -1;
	}

#ifdef PERFORMANCE_HSF
	do_gettimeofday(&time);
	ms_1 = time.tv_sec + time.tv_usec;
	ms = ms_1 - ms_0;
	dev_info(cam->dev, "hsf initial time %d us\n", ms);
#endif

	share_buf->cq_size = CQ_BUF_SIZE;
	share_buf->enable_raw = pipe->enabled_raw;
	share_buf->cam_module = raw_id;
	share_buf->cam_tg = raw_id;

#ifdef PERFORMANCE_HSF
	do_gettimeofday(&time);
	ms_0 = time.tv_sec + time.tv_usec;
#endif

	//FIXME:
	dma_map_cq = hsf_config->cq_buf;
	dma_map_chk = hsf_config->chk_buf;
	//TEST: secure iova map to camsys device.
	dma_map_cq->dbuf = mtk_cam_dmabuf_alloc(ctx, CQ_BUF_SIZE);
	dev = mtk_cam_find_raw_dev_hsf(cam, ctx->pipe->enabled_raw);
	if (dev == NULL) {
		dev_info(cam->dev, "Get dev failed\n");
		return -1;
	}
	raw_dev = dev_get_drvdata(dev);
	dev_info(cam->dev, "mtk_cam_dmabuf_alloc Done\n");
	if (mtk_cam_dmabuf_get_iova(ctx, &(hsf_config->ccu_pdev->dev), dma_map_cq) != 0) {
		dev_info(cam->dev, "Get cq iova failed\n");
		return -1;
	}
	share_buf->cq_dst_iova = dma_map_cq->dma_addr;
	dev_info(cam->dev, "dma_map_cq->dma_addr:0x%lx dma_map_cq->hsf_handle:0x%lx\n",
		dma_map_cq->dma_addr, dma_map_cq->hsf_handle);
	//TEST: secure map to ccu devcie.
	dma_map_chk->dbuf = mtk_cam_dmabuf_alloc(ctx, CQ_BUF_SIZE);
	dev_info(cam->dev, "mtk_cam_dmabuf_alloc ccu Done\n");
	if (mtk_cam_dmabuf_get_iova(ctx, &(hsf_config->ccu_pdev->dev), dma_map_chk) != 0) {
		dev_info(cam->dev, "Get chk iova failed\n");
		return -1;
	}
	dev_info(cam->dev, "dma_map_chk->dma_addr:0x%lx dma_map_chk->hsf_handle:0x%lx\n",
		dma_map_chk->dma_addr, dma_map_chk->hsf_handle);
	share_buf->chunk_hsfhandle = dma_map_chk->hsf_handle;
	share_buf->chunk_iova =  dma_map_chk->dma_addr;
	arm_smccc_smc(MTK_SIP_KERNEL_DAPC_CAM_CONTROL, 1, 0, 0, 0, 0, 0, 0, &res);
	ccu_hsf_config(ctx, 1);

//enable devapc

#ifdef PERFORMANCE_HSF
	do_gettimeofday(&time);
	ms_1 = time.tv_sec + time.tv_usec;
	ms = ms_1 - ms_0;
	dev_info(cam->dev, "hsf config time %d us\n", ms);
#endif
#ifdef PERFORMANCE_HSF
	do_gettimeofday(&time);
	ms_0 = time.tv_sec + time.tv_usec;
#endif

#ifdef PERFORMANCE_HSF
	do_gettimeofday(&time);
	ms_1 = time.tv_sec + time.tv_usec;
	ms = ms_1 - ms_0;
	dev_info(cam->dev, "atf devapc enable time %d us\n", ms);
#endif

	return 0;
}

int mtk_cam_hsf_uninit(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_hsf_ctrl *hsf_config = NULL;
	int ret = 0;
	struct arm_smccc_res res;
#ifdef PERFORMANCE_HSF
	int ms_0 = 0, ms_1 = 0, ms = 0;
	struct timeval time
;
	do_gettimeofday(&time);
	ms_0 = time.tv_sec + time.tv_usec;
#endif
	hsf_config = ctx->hsf;
	if (hsf_config == NULL) {
		pr_info("%s error: hsf_config is NULL pointer\n", __func__);
		return -1;
	}

	if (ret != 0) {
		dev_info(cam->dev, "HSF camera unint fail ret = %d\n", ret);
		return -1;
	}
	arm_smccc_smc(MTK_SIP_KERNEL_DAPC_CAM_CONTROL, 0, 0, 0, 0, 0, 0, 0, &res);

	ccu_hsf_config(ctx, 0);
	mtk_cam_dmabuf_free_iova(ctx, hsf_config->cq_buf);
	mtk_cam_dmabuf_free_iova(ctx, hsf_config->chk_buf);

#ifdef USING_CCU
	mtk_cam_power_on_ccu(hsf_config, 0);
#endif
#ifdef PERFORMANCE_HSF
	do_gettimeofday(&time);
	ms_1 = time.tv_sec + time.tv_usec;
	ms = ms_1 - ms_0;
	dev_info(cam->dev, "%s %d us\n", __func__, ms);
#endif
	kfree(hsf_config->share_buf);
	kfree(hsf_config->cq_buf);
	kfree(hsf_config->chk_buf);
	kfree(ctx->hsf);
	return 0;
}


