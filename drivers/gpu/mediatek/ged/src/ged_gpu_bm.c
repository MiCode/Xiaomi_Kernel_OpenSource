// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/init.h>

#include <ged_gpu_bm.h>

#if defined(MTK_GPU_BM_2)
#include <gpu_bm.h>
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_reservedmem_define.h>
static phys_addr_t rec_phys_addr, rec_virt_addr;
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

static void get_rec_addr(void)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	int i;
	unsigned char *ptr;

	/* get sspm reserved mem */
	rec_phys_addr = sspm_reserve_mem_get_phys(GPU_MEM_ID);
	rec_virt_addr = sspm_reserve_mem_get_virt(GPU_MEM_ID);
	rec_size = sspm_reserve_mem_get_size(GPU_MEM_ID);
	/* clear */
	ptr = (unsigned char *)(uintptr_t)rec_virt_addr;
	for (i = 0; i < rec_size; i++)
		ptr[i] = 0x0;

	   gpu_info_ref = (struct v1_data *)(uintptr_t)rec_virt_addr;
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
