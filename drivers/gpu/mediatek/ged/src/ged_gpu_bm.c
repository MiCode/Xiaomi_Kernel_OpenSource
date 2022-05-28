// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include <ged_gpu_bm.h>
#include <ged_base.h>

#if defined(MTK_GPU_BM_2)
#include <gpu_bm.h>
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_reservedmem_define.h>
static unsigned int g_qos_sysram_support;
static phys_addr_t rec_phys_addr, rec_virt_addr;
static void __iomem *mtk_sspm_bm_sysram_base_addr;
static unsigned long long rec_size;
struct v1_data *gpu_info_ref;
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

static struct job_status_qos addr;
static struct v1_data *v1;
static int sf_pid;
static int is_gpu_bm_inited;

static unsigned int qos_frame_nr;
#endif /* MTK_GPU_BM_2 */

#if defined(MTK_GPU_BM_2)

static int add_check_ovf(int v1, int v2)
{
	if (v1 > INT_MAX - v2)
		return 1;
	else
		return v1 + v2;
}

static void __iomem *_gpu_bm_of_ioremap(void)
{
	struct device_node *node = NULL;
	void __iomem *mapped_addr = NULL;
	struct resource res;
	int ret = 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,gpu_fdvfs");
	if (node) {
		mapped_addr = of_iomap(node, 0);
		GED_LOGI("[GPU_QOS]mapped_addr: %p", mapped_addr);
		of_node_put(node);
		ret = of_address_to_resource(node, 0, &res);
		rec_phys_addr = res.start;
		if (ret)
			GED_LOGE("[GPU_QOS]Cannot get physical memory addr");
		GED_LOGI("[GPU_QOS] get physical memory addr: %x", (unsigned int)rec_phys_addr);
	} else {
		GED_LOGE("[GPU_QOS]Cannot find [gpu_fdvfs] of_node");
	}

	return mapped_addr;
}

static void check_sysram_support(void)
{
	struct device_node *gpu_qos_node = NULL;
	int ret = 0;

	gpu_qos_node = of_find_compatible_node(NULL, NULL, "mediatek,gpu_qos");
	if (unlikely(!gpu_qos_node)) {
		GED_LOGE("[GPU_QOS]Failed to find gpu_qos node");
	} else {
		of_property_read_u32(gpu_qos_node, "qos-sysram-support", &g_qos_sysram_support);
		if (!g_qos_sysram_support)
			GED_LOGI("[GPU_QOS] sysram not support");
	}
	GED_LOGI("[GPU_QOS] sysram support: %d", g_qos_sysram_support);
}

static void get_rec_addr(void)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	int i;
	unsigned char *ptr;

	check_sysram_support();
	if (!g_qos_sysram_support) {
		GED_LOGI("[GPU_QOS] DRAM");
		/* get sspm reserved mem */
		rec_phys_addr = sspm_reserve_mem_get_phys(GPU_MEM_ID);
		rec_virt_addr = sspm_reserve_mem_get_virt(GPU_MEM_ID);
		rec_size = sspm_reserve_mem_get_size(GPU_MEM_ID);
	} else {
		/* get sysram address (with fastdvfs and power_model) */
		GED_LOGI("[GPU_QOS] SYSRAM");
		mtk_sspm_bm_sysram_base_addr = _gpu_bm_of_ioremap();
		rec_virt_addr = mtk_sspm_bm_sysram_base_addr;
		rec_size = NR_BM_COUNTER;
	}

	if (rec_virt_addr) {
		/* clear */
		ptr = (unsigned char *)(uintptr_t)rec_virt_addr;
		for (i = 0; i < rec_size; i++)
			ptr[i] = 0x0;

		gpu_info_ref = (struct v1_data *)(uintptr_t)rec_virt_addr;
	}

#endif
}

int mtk_bandwidth_resource_init(void)
{
	int err = 0;

	get_rec_addr();
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	if (gpu_info_ref == NULL) {
		err = -1;
		pr_info("%s: get sspm reserved memory fail\n", __func__);
		return err;
	}
	v1 = gpu_info_ref;
	v1->version = 1;
	v1->ctx = 0;
	v1->frame = 0;
	v1->job = 0;
	addr.phyaddr = rec_phys_addr;

	MTKGPUQoS_setup(v1, addr.phyaddr, rec_size);
	is_gpu_bm_inited = 1;

	return err;
#endif
	return -1;
}
EXPORT_SYMBOL(mtk_bandwidth_resource_init);

void mtk_bandwidth_update_info(int pid, int frame_nr, int job_id)
{
	if (pid == sf_pid || !is_gpu_bm_inited)
		return;

	v1->ctx = (u32)pid;
	v1->frame = (u32)frame_nr;
	v1->job = (u32)job_id;

	MTKGPUQoS_mode();
}

void mtk_bandwidth_check_SF(int pid, int isSF)
{
	if (isSF == 1 && pid != sf_pid)
		sf_pid = pid;
}

u32 qos_inc_frame_nr(void)
{
	qos_frame_nr = add_check_ovf(qos_frame_nr, 1);
	return qos_frame_nr;
}

u32 qos_get_frame_nr(void)
{
	return qos_frame_nr;
}
#endif /* MTK_GPU_BM_2 */
