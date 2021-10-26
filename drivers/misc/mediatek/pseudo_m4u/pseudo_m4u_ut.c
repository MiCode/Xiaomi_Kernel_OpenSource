/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "m4u_test: " fmt

#include <linux/iommu.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/dma-iommu.h>
#include <soc/mediatek/smi.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/device.h>
#include <linux/memblock.h>
#include <asm/cacheflush.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/fb.h>
#include <mt-plat/aee.h>
#include <linux/pagemap.h>
#include <linux/compat.h>
#include <linux/sched/signal.h>
#include <linux/sched/clock.h>
#include <asm/dma-iommu.h>
#include <sync_write.h>

#include "ion_drv.h"
#include "mtk_ion.h"
#include "mach/mt_iommu.h"
#include "mtk_iommu_ext.h"

struct m4u_ion_data {
	struct device			*dev;
};

struct test_buffer_info {
	struct ion_client *client;
	struct ion_handle *handle;
	dma_addr_t mva;
	size_t size;
};

struct m4u_ion_data *m4u_data;

struct test_buffer_info *m4u_test_start(size_t size, int port)
{
	struct ion_mm_data mm_data;
	struct ion_client *client = NULL;
	struct ion_handle *handle = NULL;
	struct test_buffer_info *buf_info = NULL;
	size_t mva_size = 0;
	ion_phys_addr_t buffer_mva = 0;

	memset((void *)&mm_data, 0, sizeof(mm_data));
	if (!g_ion_device) {
		pr_notice("%s:g_ion_device is NULL\n", __func__);
		return NULL;
	}

	client = ion_client_create(g_ion_device, "m4u_test_client");

	buf_info = kzalloc(sizeof(*buf_info), GFP_KERNEL);
	if (!buf_info)
		return NULL;

	handle = ion_alloc(client, size, 0, ION_HEAP_MULTIMEDIA_MASK, 0);
	if (IS_ERR(handle)) {
		pr_notice("Fatal Error, ion_alloc for size %d failed\n",
			  size);
		goto err;
	}

	mm_data.config_buffer_param.kernel_handle = handle;
	mm_data.config_buffer_param.module_id = port;
	mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
	if (ion_kernel_ioctl(client, ION_CMD_MULTIMEDIA,
			     (unsigned long)&mm_data) < 0) {
		pr_notice("m4u_test_drv: Config buffer failed.\n");
		goto err;
	}

	ion_phys(client, handle, &buffer_mva, &mva_size);
	if (buffer_mva == 0) {
		pr_notice("Fatal Error, get mva failed\n");
		goto err;
	}

	buf_info->client = client;
	buf_info->handle = handle;
	buf_info->mva = (dma_addr_t)buffer_mva;
	buf_info->size = mva_size;

	pr_info("%s done mva:%pa, size:0x%zx, port:%s\n",
		__func__, &buf_info->mva, buf_info->size,
		iommu_get_port_name(port));
	return buf_info;

err:
	ion_free(client, handle);
	ion_client_destroy(client);

	return NULL;
}

int m4u_test_end(struct test_buffer_info *buf_info)
{
	ion_free(buf_info->client, buf_info->handle);
	ion_client_destroy(buf_info->client);
	kfree(buf_info);
	pr_info("%s done\n", __func__);

	return 0;
}

int m4u_test(size_t size, int port)
{
	int ret = 0;
	struct test_buffer_info *buf_info;

	buf_info = m4u_test_start(size, port);
	if (!buf_info) {
		pr_notice("%s fail, port:%s\n", __func__,
			  iommu_get_port_name(port));
		return -1;
	}
	//ret = m4u_test_end(buf_info);

	return ret;
}

static int m4u_test_probe(struct platform_device *pdev)
{
#if 0 /* avoid build error */
	int ret;

	m4u_data = kzalloc(sizeof(*m4u_data), GFP_KERNEL);
	if (!m4u_data)
		return -ENOMEM;

	m4u_data->dev = &pdev->dev;

	ret = m4u_test(12345, M4U_PORT_L0_DISP_POSTMASK0); //boundary0
	ret = m4u_test(12345, M4U_PORT_L2_MDP_DISP_FAKE0);
	ret = m4u_test(12345, M4U_PORT_L4_VDEC_MC_EXT); //boundary1
	ret = m4u_test(12345, M4U_PORT_L2_MDP_RDMA0); //boundary2
	ret = m4u_test(12345, M4U_PORT_L22_CCU0); //ccu0
	ret = m4u_test(12345, M4U_PORT_L23_CCU1); //ccu1
	ret = m4u_test(12345, M4U_PORT_L13_CAM_CCUI); //ccui
	ret = m4u_test(12345, M4U_PORT_L14_CAM_CCUO); //ccuo
	ret = m4u_test(12345, M4U_PORT_L21_APU_FAKE_CODE); //apu_code
	ret = m4u_test(12345, M4U_PORT_L21_APU_FAKE_DATA); //apu_data
	ret = m4u_test(12345, M4U_PORT_L21_APU_FAKE_VLM); //apu_vlm
#endif
	pr_info("probe dev:0x%lx, name:%s\n",
		(unsigned long)&pdev->dev, dev_name(&pdev->dev));

	return 0;
}

static int m4u_test_remove(struct platform_device *pdev)
{
	return 0;
}

/*
 * m4u_boundary0: mtk_display {
 *	compatible = "mediatek,boundary0";
 *	iommus = <&iommu0 M4U_PORT_L0_DISP_POSTMASK0>;
 * };
 * m4u_boundary1: mtk_video {
 *	compatible = "mediatek,boundary1";
 *	iommus = <&iommu0 M4U_PORT_L7_VENC_RCPU>;
 * };
 * m4u_boundary2: mtk_cam {
 *	compatible = "mediatek,boundary2";
 *	iommus = <&iommu0 M4U_PORT_L2_MDP_RDMA0>;
 * };
 * m4u_boundary3: mtk_apu {
 *	compatible = "mediatek,boundary3";
 *	iommus = <&iommu0 M4U_PORT_L21_APU_FAKE_DATA>;
 * };
 * m4u_ccu_L22: mtk_ccu_L22 {c
 *	compatible = "mediatek,mtk_ccu_l22";
 *	iommus = <&iommu0 M4U_PORT_L22_CCU>;
 * };
 * m4u_ccu_L23: mtk_ccu_L23 {
 *	compatible = "mediatek,mtk_ccu_l23";
 *	iommus = <&iommu0 M4U_PORT_L23_CCU>;
 * };
 * m4u_apu_code: mtk_apu_code {
 *	compatible = "mediatek,mtk_apu_code";
 *	iommus = <&iommu0 M4U_PORT_L21_APU_FAKE_CODE>;
 * };
 * m4u_apu_vlm: mtk_apu_vlm {
 *	compatible = "mediatek,mtk_apu_vlm";
 *	iommus = <&iommu0 M4U_PORT_L21_APU_FAKE_VLM>;
 * };
 */
static const struct of_device_id m4u_test_match_table[] = {
	{.compatible = "mediatek,boundary0"},
	//{.compatible = "mediatek,boundary1"},
	//{.compatible = "mediatek,boundary2"},
	//{.compatible = "mediatek,boundary3"},
	//{.compatible = "mediatek,mtk_ccu_l22"},
	//{.compatible = "mediatek,mtk_ccu_l23"},
	//{.compatible = "mediatek,mtk_apu_code"},
	//{.compatible = "mediatek,mtk_apu_vlm"},
	{},
};

static struct platform_driver m4u_test_driver = {
	.probe = m4u_test_probe,
	.remove = m4u_test_remove,
	.driver = {
		.name = "m4u-test",
		.of_match_table = m4u_test_match_table,
	},
};

static int __init m4u_test_init(void)
{
	pr_info("%s()\n", __func__);
	if (platform_driver_register(&m4u_test_driver)) {
		pr_notice("%s platform driver register failed.\n", __func__);
		return -ENODEV;
	}
	return 0;
}

static void __exit m4u_test_exit(void)
{
	platform_driver_unregister(&m4u_test_driver);
}

module_init(m4u_test_init);
module_exit(m4u_test_exit);
