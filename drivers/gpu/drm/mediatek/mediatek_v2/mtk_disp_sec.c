// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include <drm/drm_crtc.h>
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_log.h"
#include "mtk_dump.h"


#include "cmdq-sec.h"
#include "cmdq-sec-iwc-common.h"
#include "cmdq-sec-mailbox.h"
#include "mtk_heap.h"
#include "mtk_drm_gem.h"
#include "mtk-cmdq-ext.h"

#define RETRY_SEC_CMDQ_FLUSH 3

struct mtk_disp_sec_config {
	struct cmdq_client *disp_sec_client;
	struct cmdq_pkt *sec_cmdq_handle;
	u32 tzmp_disp_sec_wait;
	u32 tzmp_disp_sec_set;
};

struct mtk_disp_sec_config sec_config;
struct mtk_disp_sec_config mtee_config;

static void mtk_disp_secure_cb(struct cmdq_cb_data data)
{
	if (!sec_config.sec_cmdq_handle) {
		DDPINFO("%s: secure sec_cmdq_handle already stopped\n", __func__);
		return;
	}
	DDPINFO("%s err:%d\n", __func__, data.err);

	cmdq_pkt_destroy(sec_config.sec_cmdq_handle);
	sec_config.sec_cmdq_handle = NULL;
}

int mtk_disp_cmdq_secure_init(void)
{
	if (!sec_config.disp_sec_client) {
		DDPINFO("%s:%d, sec_config.disp_sec_client is NULL\n",
			__func__, __LINE__);
		return false;
	}

	if (sec_config.sec_cmdq_handle) {
		DDPMSG("%s cmdq_handle is already exist\n", __func__);
		return false;
	}

	sec_config.sec_cmdq_handle =
		cmdq_pkt_create(sec_config.disp_sec_client);

	cmdq_sec_pkt_set_data(sec_config.sec_cmdq_handle, 0, 0,
		CMDQ_SEC_DEBUG, CMDQ_METAEX_TZMP);
	cmdq_sec_pkt_set_mtee(sec_config.sec_cmdq_handle, true);
	cmdq_pkt_finalize_loop(sec_config.sec_cmdq_handle);
	if (cmdq_pkt_flush_threaded(sec_config.sec_cmdq_handle,
		mtk_disp_secure_cb, NULL) < 0) {
		DDPINFO("Failed to flush user_cmd\n");
		return false;
	}
	return true;
}

int mtk_disp_cmdq_secure_end(void)
{
	if (!sec_config.sec_cmdq_handle) {
		DDPINFO("%s cmdq_handle is NULL\n", __func__);
		return false;
	}

	if (!sec_config.disp_sec_client) {
		DDPINFO("%s:%d, sec_config.disp_sec_client is NULL\n",
			__func__, __LINE__);
		return false;
	}

	cmdq_sec_mbox_stop(sec_config.disp_sec_client);
	return true;
}

int mtk_disp_secure_domain_enable(struct cmdq_pkt *handle,
									resource_size_t dummy_larb)
{
	int retry_count = 0;
	DDPINFO("%s\n", __func__);

	while (!sec_config.sec_cmdq_handle) {
		retry_count++;
		DDPMSG("%s: no secure sec_cmdq_handle, retry %d\n"
				, __func__, retry_count);
		mtk_disp_cmdq_secure_init();
		if (retry_count > RETRY_SEC_CMDQ_FLUSH) {
			DDPMSG("%s: too many times init fail\n", __func__);
			return false;
		}
	}

	cmdq_pkt_write(handle, NULL, dummy_larb, BIT(1), BIT(1));
	cmdq_pkt_set_event(handle, sec_config.tzmp_disp_sec_wait);
	cmdq_pkt_wfe(handle, sec_config.tzmp_disp_sec_set);

	return true;
}

int mtk_disp_secure_domain_disable(struct cmdq_pkt *handle,
			resource_size_t dummy_larb)
{
	int retry_count = 0;
	DDPINFO("%s\n", __func__);

	while (!sec_config.sec_cmdq_handle) {
		retry_count++;
		DDPMSG("%s: no secure sec_cmdq_handle, retry %d\n"
				, __func__, retry_count);
		mtk_disp_cmdq_secure_init();
		if (retry_count > RETRY_SEC_CMDQ_FLUSH) {
			DDPMSG("%s: too many times init fail\n", __func__);
			return false;
		}
	}

	cmdq_pkt_write(handle, NULL, dummy_larb, 0, BIT(1));
	cmdq_pkt_set_event(handle, sec_config.tzmp_disp_sec_wait);
	cmdq_pkt_wfe(handle, sec_config.tzmp_disp_sec_set);

	return true;
}

static int mtk_drm_disp_sec_cb_event(int value, struct cmdq_pkt *handle,
				resource_size_t dummy_larb)
{
	DDPINFO("%s\n", __func__);
	switch (value) {
	case DISP_SEC_START:
		return mtk_disp_cmdq_secure_init();
	case DISP_SEC_STOP:
		return mtk_disp_cmdq_secure_end();
	case DISP_SEC_ENABLE:
		return mtk_disp_secure_domain_enable(handle, dummy_larb);
	case DISP_SEC_DISABLE:
		return mtk_disp_secure_domain_disable(handle, dummy_larb);
	default:
		return false;
	}
}


static int disp_sec_probe(struct platform_device *pdev)
{
	void **ret;
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;

	DDPMSG("%s+\n", __func__);
	sec_config.disp_sec_client = cmdq_mbox_create(dev, 0);
	if (sec_config.disp_sec_client) {
		of_property_read_u32(node, "sw_sync_token_tzmp_disp_wait",
					&sec_config.tzmp_disp_sec_wait);
		of_property_read_u32(node, "sw_sync_token_tzmp_disp_set",
					&sec_config.tzmp_disp_sec_set);
		DDPMSG("tzmp_disp_sec_wait %d tzmp_disp_sec_set %d\n",
					sec_config.tzmp_disp_sec_wait,
					sec_config.tzmp_disp_sec_set);
	}
	ret = mtk_drm_disp_sec_cb_init();
	*ret = (void *) mtk_drm_disp_sec_cb_event;
	DDPMSG("%s-\n", __func__);
	return 0;
}

static int disp_sec_remove(struct platform_device *pdev)
{
	DDPMSG("%s\n", __func__);
	return 0;
}

static const struct of_device_id of_disp_sec_match_tbl[] = {
	{
		.compatible = "mediatek,disp_sec",
	},
	{}
};

static struct platform_driver disp_sec_drv = {
	.probe = disp_sec_probe,
	.remove = disp_sec_remove,
	.driver = {
		.name = "mtk_disp_sec",
		.of_match_table = of_disp_sec_match_tbl,
	},
};

int mtk_disp_mtee_domain_enable(struct cmdq_pkt *handle, u32 regs_addr, u32 lye_addr,
				u32 offset, u32 size)
{
	DDPINFO("%s+\n", __func__);

	cmdq_sec_pkt_write_reg(handle, regs_addr, lye_addr, CMDQ_IWC_H_2_MVA, offset, size, 0);

	return true;
}

static u64 mtk_crtc_secure_port_lookup(struct mtk_ddp_comp *comp)
{
	u64 ret = 0;

	if (!comp)
		return ret;

	switch (comp->id) {
	case DDP_COMPONENT_WDMA0:
		ret = 1LL << CMDQ_SEC_DISP_WDMA0;
		break;
	case DDP_COMPONENT_WDMA1:
		ret = 1LL << CMDQ_SEC_DISP_WDMA1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

static u64 mtk_crtc_secure_dapc_lookup(struct mtk_ddp_comp *comp)
{
	u64 ret = 0;

	if (!comp)
		return ret;

	switch (comp->id) {
	case DDP_COMPONENT_WDMA0:
		ret = 1LL << CMDQ_SEC_DISP_WDMA0;
		break;
	case DDP_COMPONENT_WDMA1:
		ret = 1LL << CMDQ_SEC_DISP_WDMA1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

int mtk_disp_mtee_cmdq_secure_start(int value, struct cmdq_pkt *cmdq_handle,
				struct mtk_ddp_comp *comp, u32 crtc_id)
{
	u64 sec_disp_port, sec_disp_dapc;
	u32 sec_disp_scn;

	if (crtc_id == 0) {
		sec_disp_port = 0;
		sec_disp_dapc = 0;
		if (value == DISP_SEC_START)
			sec_disp_scn = CMDQ_SEC_PRIMARY_DISP;
		else
			sec_disp_scn = CMDQ_SEC_DISP_PRIMARY_DISABLE_SECURE_PATH;
	} else {
		sec_disp_port = (crtc_id == 1) ? 0 :
				mtk_crtc_secure_port_lookup(comp);
		sec_disp_dapc = (crtc_id == 1) ? 0 :
				mtk_crtc_secure_dapc_lookup(comp);
		if (value == DISP_SEC_START)
			sec_disp_scn = CMDQ_SEC_SUB_DISP;
		else
			sec_disp_scn = CMDQ_SEC_DISP_SUB_DISABLE_SECURE_PATH;
	}

	cmdq_sec_pkt_set_data(cmdq_handle, sec_disp_dapc,
			sec_disp_port, sec_disp_scn,
			CMDQ_METAEX_NONE);
	cmdq_sec_pkt_set_secid(cmdq_handle, 1);
	cmdq_sec_pkt_set_mtee(cmdq_handle, true);

	return true;
}

int mtk_disp_mtee_gem_fd_to_sec_hdl(u32 fd, struct mtk_drm_gem_obj *mtk_gem_obj)
{
	int sec_id = -1;
	u32 sec_handle;
	struct dma_buf *dma_buf = NULL;

	dma_buf = dma_buf_get(fd);
	if (IS_ERR(dma_buf)) {
		DDPMSG("%s dma_buf_get fail\n", __func__);
		return false;
	}

	sec_id = dmabuf_to_sec_id(dma_buf, &sec_handle);
	if (sec_id >= 0)
		DDPINFO("%s:sec_hnd=0x%x,sec_id=%d\n", __func__, sec_handle, sec_id);
	else {
		DDPMSG("%s failed %d\n", __func__, fd);
		dma_buf_put(dma_buf);
		return false;
	}
	mtk_gem_obj->dma_addr = sec_handle;

	dma_buf_put(dma_buf);
	return true;
}

static int mtk_drm_disp_mtee_cb_event(int value, int fd, struct mtk_drm_gem_obj *mtk_gem_obj,
	struct cmdq_pkt *handle, struct mtk_ddp_comp *comp, u32 crtc_id, u32 regs_addr,
	u32 lye_addr, u32 offset, u32 size)
{
	DDPINFO("%s,cmd-%d\n", __func__, value);
	switch (value) {
	case DISP_SEC_START:
		return mtk_disp_mtee_cmdq_secure_start(value, handle, comp, crtc_id);
	case DISP_SEC_STOP:
		return mtk_disp_mtee_cmdq_secure_start(value, handle, comp, crtc_id);
	case DISP_SEC_ENABLE:
		return mtk_disp_mtee_domain_enable(handle, regs_addr, lye_addr, offset, size);
	case DISP_SEC_FD_TO_SEC_HDL:
		return mtk_disp_mtee_gem_fd_to_sec_hdl(fd, mtk_gem_obj);
	default:
		return false;
	}
}

static void mtk_crtc_init_gce_sec_obj(struct drm_device *drm_dev, struct device *dev,
			struct mtk_drm_crtc *mtk_crtc)
{
	int index;
	unsigned int i = CLIENT_SEC_CFG;
	unsigned int crtc_id = drm_crtc_index(&mtk_crtc->base);

	/* Load CRTC GCE client */
	if (crtc_id == 0)
		index = of_property_match_string(dev->of_node,
						 "gce-client-names", "CLIENT_SEC_CFG0");
	else if (crtc_id == 1)
		index = of_property_match_string(dev->of_node,
						 "gce-client-names", "CLIENT_SEC_CFG1");
	else
		index = of_property_match_string(dev->of_node,
						 "gce-client-names", "CLIENT_SEC_CFG2");

	if (index < 0) {
		mtk_crtc->gce_obj.client[i] = NULL;
		DDPPR_ERR("get index failed\n");
	} else
		mtk_crtc->gce_obj.client[i] =
			cmdq_mbox_create(dev, index);

	/* support DC with color matrix config no more */
	/* mtk_crtc_init_color_matrix_data_slot(mtk_crtc); */
	mtk_crtc->gce_obj.base = cmdq_register_device(dev);

	DDPINFO("%s %d-done\n", __func__, index);
}

static int _drm_crtc_sec_create(struct drm_device *drm_dev, struct device *dev)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;

	if (IS_ERR_OR_NULL(drm_dev)) {
		DDPPR_ERR("%s, invalid drm dev\n", __func__);
		return -1;
	}

	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
			typeof(*crtc), head);

	drm_for_each_crtc(crtc, drm_dev) {
		mtk_crtc = to_mtk_crtc(crtc);
		if (!mtk_crtc) {
			DDPPR_ERR("create sec crtc failed\n");
			return -1;
		}
		mtk_crtc_init_gce_sec_obj(drm_dev, dev, mtk_crtc);
	}
	return 0;
}

static int disp_mtee_probe(struct platform_device *pdev)
{
	void **ret;
	struct device *dev = &pdev->dev;

	DDPINFO("%s+\n", __func__);

	if (IS_ERR_OR_NULL(disp_mtee_cb.dev))
		DDPPR_ERR("mtee_probe:invalid dev\n");
	else {
		if (_drm_crtc_sec_create(disp_mtee_cb.dev, dev) < 0) {
			DDPPR_ERR("%s:create sec crtc failed\n", __func__);
			return -1;
		}
	}

	ret = mtk_drm_disp_mtee_cb_init();
	*ret = (void *) mtk_drm_disp_mtee_cb_event;
	DDPINFO("%s-\n", __func__);
	return 0;
}

static int disp_mtee_remove(struct platform_device *pdev)
{
	DDPINFO("%s\n", __func__);
	return 0;
}

static const struct of_device_id of_disp_mtee_match_tbl[] = {
	{
		.compatible = "mediatek,disp_mtee",
	},
	{}
};

static struct platform_driver disp_mtee_drv = {
	.probe = disp_mtee_probe,
	.remove = disp_mtee_remove,
	.driver = {
		.name = "mtk_mtee_sec",
		.of_match_table = of_disp_mtee_match_tbl,
	},
};

static int __init mtk_disp_sec_init(void)
{
	s32 status;

	status = platform_driver_register(&disp_mtee_drv);
	if (status) {
		DDPMSG("Failed to register mtee sec driver(%d)\n", status);
		return -ENODEV;
	}

	status = platform_driver_register(&disp_sec_drv);
	if (status) {
		DDPMSG("Failed to register disp sec driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_disp_sec_exit(void)
{
	platform_driver_unregister(&disp_mtee_drv);
	platform_driver_unregister(&disp_sec_drv);
}

module_init(mtk_disp_sec_init);
module_exit(mtk_disp_sec_exit);

MODULE_AUTHOR("Aaron Chung <Aaron.Chung@mediatek.com>");
MODULE_DESCRIPTION("MTK DRM secure Display");
MODULE_LICENSE("GPL v2");
