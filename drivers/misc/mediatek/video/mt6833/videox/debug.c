/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include <linux/string.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/sched.h>
#ifdef CONFIG_MTK_M4U
#include "m4u.h"
#endif
#include "ddp_m4u.h"
#include "disp_drv_log.h"
#include "mtkfb.h"
#include "debug.h"
#include "lcm_drv.h"
#include "ddp_ovl.h"
#include "ddp_path.h"
#include "ddp_reg.h"
#include "primary_display.h"
#include "mtk_disp_mgr.h"
#include "display_recorder.h"
#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#else
#include "disp_dts_gpio.h"
#endif
#include "mtkfb_fence.h"
#include "disp_helper.h"
#include "ddp_manager.h"
#include "ddp_log.h"
#include "ddp_dsi.h"

#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"
#include "disp_lowpower.h"
#include "disp_arr.h"
#include "disp_recovery.h"
#include "disp_partial.h"
#include "disp_drv_platform.h"
#if defined(CONFIG_MTK_IOMMU_V3)
#include "mtk_ion.h"
#include "ion_drv.h"
#include "ion.h"
#endif
#include "layering_rule.h"
#include "ddp_clkmgr.h"

static struct dentry *mtkfb_dbgfs;
unsigned int g_mobilelog = 1;
int bypass_blank;
int lcm_mode_status;
int layer_layout_allow_non_continuous;
/* Boundary of enter screen idle */
unsigned long long idle_check_interval = 50;
/* modify rdma threshold for debug */
int dbg_ultlow, dbg_ulthigh, dbg_prehigh, dbg_urg_low, dbg_urg_high;

/* hrt */
unsigned long long hrt_high, hrt_low;
int hrt_show_flag;

struct BMP_FILE_HEADER {
	UINT16 bfType;
	UINT32 bfSize;
	UINT16 bfReserved1;
	UINT16 bfReserved2;
	UINT32 bfOffBits;
};

struct BMP_INFO_HEADER {
	UINT32 biSize;
	UINT32 biWidth;
	UINT32 biHeight;
	UINT16 biPlanes;
	UINT16 biBitCount;
	UINT32 biCompression;
	UINT32 biSizeImage;
	UINT32 biXPelsPerMeter;
	UINT32 biYPelsPerMeter;
	UINT32 biClrUsed;
	UINT32 biClrImportant;
};

/*********************** layer information statistic *********************/
#define STATISTIC_MAX_LAYERS 20
struct layer_statistic {
	unsigned long total_frame_cnt;
	unsigned long cnt_by_layers[STATISTIC_MAX_LAYERS];
	unsigned long cnt_by_layers_with_ext[STATISTIC_MAX_LAYERS];
	unsigned long cnt_by_layers_with_arm_ext[STATISTIC_MAX_LAYERS];
};
static struct layer_statistic layer_stat;
static int layer_statistic_enable;

static int _is_overlap(unsigned int x1, unsigned int y1,
	unsigned int w1, unsigned int h1, unsigned int x2,
	unsigned int y2, unsigned int w2, unsigned int h2)
{
	if (x2 >= x1 + w1 || x1 >= x2 + w2)
		return 0;
	if (y2 >= y1 + h1 || y1 >= y2 + h2)
		return 0;
	return 1;
}

static int layer_is_overlap(struct disp_frame_cfg_t *cfg,
	int idx, int from, int to)
{
	int i;

	for (i = from; i <= to; i++) {
		if (_is_overlap(cfg->input_cfg[idx].tgt_offset_x,
			cfg->input_cfg[idx].tgt_offset_y,
			cfg->input_cfg[idx].src_width,
			cfg->input_cfg[idx].src_height,
			cfg->input_cfg[i].tgt_offset_x,
			cfg->input_cfg[i].tgt_offset_y,
			cfg->input_cfg[i].src_width,
			cfg->input_cfg[i].src_height))
			return 1;
	}
	return 0;
}


static int calc_layer_num_with_arm_ext(struct disp_frame_cfg_t *cfg)
{
	int ovl_phy_num[2] = {4, 2};
	int ovl_ext_num[2] = {3, 3};
	int ovl_idx = 0;
	int i, cur_phy_num, cur_ext_num;
	int cur_phy_idx_in_cfg;
	int total_phy_layer = 0;

	cur_phy_num = 0;
	cur_ext_num = 0;
	cur_phy_idx_in_cfg = 0;
	for (i = 0; i < cfg->input_layer_num; i++) {
		int is_overlap;

		if (!cfg->input_cfg[i].layer_enable)
			continue;

		if (cur_phy_num && cur_ext_num < ovl_ext_num[ovl_idx])
			is_overlap = layer_is_overlap(cfg, i,
				cur_phy_idx_in_cfg, i - 1);
		else
			is_overlap = 1;

		if (!is_overlap) {
			/* put it in ext layer */
			cur_ext_num++;
			continue;
		}

		/* now put it into a phy layer */
		if (cur_phy_num < ovl_phy_num[ovl_idx]) {
			cur_phy_num++;
			cur_phy_idx_in_cfg = i;
		} else if (ovl_idx < ARRAY_SIZE(ovl_phy_num)) {
			/* dispatch to next ovl */
			ovl_idx++;
			cur_phy_num = 1;
			cur_phy_idx_in_cfg = i;
			cur_ext_num = 0;
		} else {
			/* no ovl layer aviable !! */
			goto err_out;
		}
	}

	for (i = 0; i < ovl_idx; i++)
		total_phy_layer += ovl_phy_num[i];
	total_phy_layer += cur_phy_num;
	return total_phy_layer;

err_out:
	DISPWARN("%s failed: ovl_idx=%d, cur_phy=%d, cur_ext=%d\n",
		__func__, ovl_idx, cur_phy_num, cur_ext_num);
	for (i = 1; i < cfg->input_layer_num; i++)
		dump_input_cfg_info(&cfg->input_cfg[i],
			MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0), 1);

	return -1;
}

int disp_layer_info_statistic(struct disp_ddp_path_config *last_config,
	struct disp_frame_cfg_t *cfg)
{
	unsigned int i, phy_num = 0, ext_num = 0;
	int phy_num_with_arm_ext;

	if (!READ_ONCE(layer_statistic_enable))
		return 0;

	layer_stat.total_frame_cnt++;

	for (i = 0; i < cfg->input_layer_num; i++) {
		if (!cfg->input_cfg[i].layer_enable)
			continue;
		if (cfg->input_cfg[i].ext_sel_layer != -1)
			ext_num++;
		else
			phy_num++;
	}
	layer_stat.cnt_by_layers[phy_num + ext_num]++;
	layer_stat.cnt_by_layers_with_ext[phy_num]++;

	phy_num_with_arm_ext = calc_layer_num_with_arm_ext(cfg);
	if (phy_num_with_arm_ext > 0) {
		phy_num_with_arm_ext =
			min(phy_num_with_arm_ext, STATISTIC_MAX_LAYERS);
		layer_stat.cnt_by_layers_with_arm_ext[phy_num_with_arm_ext]++;
	}

	if (!(layer_stat.total_frame_cnt % 100)) {
		char str[200];
		int offset = 0;

		offset += snprintf(str + offset, sizeof(str) - offset,
			"total:%ld.layers:", layer_stat.total_frame_cnt);
		for (i = 1; i <= 12; i++)
			offset += snprintf(str + offset, sizeof(str) - offset,
				"%ld,", layer_stat.cnt_by_layers[i]);
		DISPMSG("layer_cnt %s\n", str);

		offset = 0;
		offset += snprintf(str + offset,
			sizeof(str) - offset, ".ext:");
		for (i = 1; i <= 6 ; i++)
			offset += snprintf(str + offset, sizeof(str) - offset,
				"%ld,", layer_stat.cnt_by_layers_with_ext[i]);

		offset += snprintf(str + offset,
			sizeof(str) - offset, ".arm_ext:");
		for (i = 1; i <= 6 ; i++)
			offset += snprintf(str + offset, sizeof(str) - offset,
			"%ld,", layer_stat.cnt_by_layers_with_arm_ext[i]);
		DISPMSG("layer_cnt %s\n", str);
	}

	return 0;
}

void disp_layer_info_statistic_reset(void)
{
	memset(&layer_stat, 0, sizeof(layer_stat));
}

/*********************** basic test ****************************/
static int basic_test_cancel;
static int draw_buffer(char *va, int w, int h,
	enum UNIFIED_COLOR_FMT ufmt, char r, char g, char b, char a)
{
	int i, j;
	int Bpp = UFMT_GET_Bpp(ufmt);

	for (i = 0; i < h; i++)
		for (j = 0; j < w; j++) {
			int x = j * Bpp + i * w * Bpp;

			if (ufmt == UFMT_RGB888 || ufmt == UFMT_RGBA8888) {
				va[x++] = r;
				va[x++] = g;
				va[x++] = b;
				if (Bpp == 4)
					va[x++] = a;
			}

			if (ufmt == UFMT_RGB565) {
				va[x++] = (b & 0x1f) | ((g & 0x7) << 5);
				va[x++] = (g & 0x7) | (r & 0x1f);
			}
		}
	return 0;
}

void save_bmp(const char *file_name, void *buf, int w, int h)
{
	struct file *bmp;
	mm_segment_t fs;
	loff_t pos = 0;
	int size = w * h * 3;
	struct BMP_FILE_HEADER bfh;
	struct BMP_INFO_HEADER bih;

	bfh.bfType = 0x4d42;
	bfh.bfSize = size + 14 + 40;
	bfh.bfReserved1 = 0;
	bfh.bfReserved2 = 0;
	bfh.bfOffBits = 54;

	bih.biSize = 40;
	bih.biWidth = w;
	bih.biHeight = h;
	bih.biPlanes = 1;
	bih.biBitCount = 24;
	bih.biCompression = 0;
	bih.biSizeImage = size;
	bih.biXPelsPerMeter = 2835;
	bih.biYPelsPerMeter = 2835;
	bih.biClrUsed  = 0;
	bih.biClrImportant = 0;

	bmp = filp_open(file_name, O_CREAT | O_RDWR, 0);
	if (IS_ERR(bmp)) {
		DISPERR("open output bmp file failed!\n");
		return;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	vfs_write(bmp, (const char *)&bfh.bfType,
		sizeof(bfh.bfType), &pos);
	vfs_write(bmp, (const char *)&bfh.bfSize,
		sizeof(bfh.bfSize), &pos);
	vfs_write(bmp, (const char *)&bfh.bfReserved1,
		sizeof(bfh.bfReserved1), &pos);
	vfs_write(bmp, (const char *)&bfh.bfReserved2,
		sizeof(bfh.bfReserved2), &pos);
	vfs_write(bmp, (const char *)&bfh.bfOffBits,
		sizeof(bfh.bfOffBits), &pos);
	vfs_write(bmp, (const char *)&bih.biSize,
		sizeof(bih.biSize), &pos);
	vfs_write(bmp, (const char *)&bih.biWidth,
		sizeof(bih.biWidth), &pos);
	vfs_write(bmp, (const char *)&bih.biHeight,
		sizeof(bih.biHeight), &pos);
	vfs_write(bmp, (const char *)&bih.biPlanes,
		sizeof(bih.biPlanes), &pos);
	vfs_write(bmp, (const char *)&bih.biBitCount,
		sizeof(bih.biBitCount), &pos);
	vfs_write(bmp, (const char *)&bih.biCompression,
		sizeof(bih.biCompression), &pos);
	vfs_write(bmp, (const char *)&bih.biSizeImage,
		sizeof(bih.biSizeImage), &pos);
	vfs_write(bmp, (const char *)&bih.biXPelsPerMeter,
		sizeof(bih.biXPelsPerMeter), &pos);
	vfs_write(bmp, (const char *)&bih.biYPelsPerMeter,
		sizeof(bih.biYPelsPerMeter), &pos);
	vfs_write(bmp, (const char *)&bih.biClrUsed,
		sizeof(bih.biClrUsed), &pos);
	vfs_write(bmp, (const char *)&bih.biClrImportant,
		sizeof(bih.biClrImportant), &pos);
	vfs_write(bmp, (const char *)buf, size, &pos);
	filp_close(bmp, NULL);
	set_fs(fs);
}

static void bmp_adjust(void *buf, int size, int w, int h)
{
	int hpos, vpos;
	void *temp;
	UINT8 byte;

	size = w * 3;
	temp = vmalloc(size);
	if (!temp)
		return;

	for (vpos = 0; vpos < h/2; vpos++) {
		memcpy(temp, buf + (h-vpos - 1) * size, size);
		memcpy(buf + (h-vpos - 1) * size, buf +
			vpos * size, size);
		memcpy(buf + vpos * size, temp, size);
	}
	for (vpos = 0; vpos < h; vpos++) {
		for (hpos = 0; hpos < w; hpos++) {
			byte = *(UINT8 *)(buf + (vpos * size) + hpos * 3);
			*(UINT8 *)(buf + (vpos * size) + hpos * 3) =
			*(UINT8 *)(buf + (vpos * size) + hpos * 3 + 2);
			*(UINT8 *)(buf + (vpos * size) + hpos * 3 + 2) = byte;
		}
	}
	vfree(temp);
}

struct test_buf_info {
	struct ion_client *ion_client;
#ifdef CONFIG_MTK_M4U
	struct m4u_client_t *m4u_client;
#endif
	struct ion_handle *handle;
	size_t size;
	void *buf_va;
	dma_addr_t buf_pa;
	unsigned long buf_mva;
};

static int alloc_buffer_from_ion(size_t size, struct test_buf_info *buf_info)
{
#if defined(CONFIG_MTK_IOMMU_V3)
	struct ion_client *client;
	struct ion_mm_data mm_data;
	struct ion_handle *handle;

	client = ion_client_create(g_ion_device, "disp_test");
	buf_info->ion_client = client;

	memset((void *)&mm_data, 0, sizeof(struct ion_mm_data));

	handle = ion_alloc(client, size, 0, ION_HEAP_MULTIMEDIA_MASK, 0);
	if (IS_ERR(buf_info->handle)) {
		DISPWARN("Fatal Error, ion_alloc for size %zu failed\n", size);
		ion_client_destroy(client);
		return -1;
	}

	buf_info->buf_va = ion_map_kernel(client, handle);
	if (buf_info->buf_va == NULL) {
		DISPWARN("ion_map_kernrl failed\n");
		ion_free(client, handle);
		ion_client_destroy(client);
		return -1;
	}

	/* use get_iova replace config_buffer & get_phys*/
	mm_data.config_buffer_param.kernel_handle = handle;
	mm_data.mm_cmd = ION_MM_GET_IOVA;
	if (ion_kernel_ioctl(client, ION_CMD_MULTIMEDIA,
		(unsigned long)&mm_data) < 0) {
		DISPWARN("ion_test_drv: get pa failed.\n");
		ion_free(client, handle);
		ion_client_destroy(client);
		return -1;
	}

	buf_info->buf_mva = (unsigned int)mm_data.get_phys_param.phy_addr;
	if (buf_info->buf_mva == 0) {
		DISPWARN("Fatal Error, get mva failed\n");
		ion_free(client, handle);
		ion_client_destroy(client);
		return -1;
	}

	buf_info->handle = handle;
#endif
	return 0;
}

static int alloc_buffer_from_dma(size_t size, struct test_buf_info *buf_info)
{
	int ret = 0;
	unsigned long size_align;

#ifdef CONFIG_MTK_M4U
	unsigned int mva = 0;

	size_align = round_up(size, PAGE_SIZE);

	arch_setup_dma_ops(disp_get_device(), 0, 0, NULL, false);

	buf_info->buf_va = dma_alloc_coherent(disp_get_device(),
		size, &buf_info->buf_pa, GFP_KERNEL);
	if (!(buf_info->buf_va)) {
		DISPMSG(
			"dma_alloc_coherent error!  dma memory not available. size=%zu\n",
			size);
		return -1;
	}
	buf_info->size = size;

	if (disp_helper_get_option(DISP_OPT_USE_M4U)) {
		static struct sg_table table;
		struct sg_table *sg_table = &table;

		ret = sg_alloc_table(sg_table, 1, GFP_KERNEL);
		if (ret) {
			DISPERR("allocate sg table failed: %d\n", ret);
			return ret;
		}

		sg_dma_address(sg_table->sgl) = buf_info->buf_pa;
		sg_dma_len(sg_table->sgl) = size_align;
		buf_info->m4u_client = m4u_create_client();
		if (IS_ERR_OR_NULL(buf_info->m4u_client))
			DISPWARN("create client fail!\n");

		ret = m4u_alloc_mva(buf_info->m4u_client,
			DISP_M4U_PORT_DISP_OVL0, 0, sg_table, size_align,
			M4U_PROT_READ | M4U_PROT_WRITE, 0, &mva);
		if (ret)
			DISPWARN("m4u_alloc_mva returns fail: %d\n", ret);
	}
	buf_info->buf_mva = mva;
	DISPMSG("%s MVA is 0x%x PA is 0x%pa\n",
		__func__, mva, &buf_info->buf_pa);
	return ret;

#else

	struct ion_client *ion_display_client = NULL;
	struct ion_handle *ion_display_handle = NULL;
	unsigned long mva = 0;

	size_align = round_up(size, PAGE_SIZE);
	ion_display_client = disp_ion_create("disp_cap_ovl");
	if (ion_display_client == NULL) {
		DISPWARN("primary capture:Fail to create ion\n");
		ret = -1;
		goto out;
	}

	ion_display_handle = disp_ion_alloc(ion_display_client,
		ION_HEAP_MULTIMEDIA_PA2MVA_MASK, buf_info->buf_pa,
		size_align);
	if (ret != 0) {
		DISPWARN("primary capture:Fail to allocate buffer\n");
		ret = -1;
		goto out;
	}
	disp_ion_get_mva(ion_display_client, ion_display_handle,
		&mva, 0, DISP_M4U_PORT_DISP_WDMA0);

out:
	buf_info->buf_mva = mva;
	DISPMSG("%s MVA is 0x%lx PA is 0x%pa\n",
		__func__, mva, &buf_info->buf_pa);
	return ret;
#endif
}

static int release_test_buf(struct test_buf_info *buf_info)
{
#if defined(CONFIG_MTK_IOMMU_V3)
	if (disp_helper_get_option(DISP_OPT_USE_M4U)) {
		/* ion buffer */
		if (buf_info->handle)
			ion_free(buf_info->ion_client, buf_info->handle);
		else
			dma_free_coherent(disp_get_device(), buf_info->size,
				buf_info->buf_va, buf_info->buf_pa);

#ifdef CONFIG_MTK_M4U
		if (buf_info->m4u_client)
			m4u_destroy_client(buf_info->m4u_client);
#endif
		if (buf_info->ion_client)
			ion_client_destroy(buf_info->ion_client);
	}
#ifndef CONFIG_MTK_IOMMU_V3
	if (!disp_helper_get_option(DISP_OPT_USE_M4U))
		dma_free_coherent(disp_get_device(), buf_info->size,
				buf_info->buf_va, buf_info->buf_pa);
#endif
#endif

	return 0;
}

static int test_alloc_buffer(size_t size, struct test_buf_info *buf_info)
{
	int ret;

	if (disp_helper_get_option(DISP_OPT_USE_M4U))
		ret = alloc_buffer_from_ion(size, buf_info);
	else
		ret = alloc_buffer_from_dma(size, buf_info);

	if (ret)
		DISPWARN("error to alloc buffer size = %lu\n",
			(unsigned long)size);
	return ret;
}

/* don't compare if cksum_golden == 0 */
static unsigned int cksum_golden;
static cmdqBackupSlotHandle cksum_slot;

static int __maybe_unused compare_dsi_checksum(unsigned long unused)
{
	unsigned int cksum;
	int ret;

	if (!cksum_golden)
		return 0;

	ret = cmdqBackupReadSlot(cksum_slot, 0, &cksum);
	if (ret) {
		DISPWARN("Fail to read cksum from cmdq slot\n");
		return -1;
	}

	if (cksum_golden != cksum)
		DISPWARN("%s fail, cksum=0x%08x, golden=0x%08x\n",
			__func__, cksum, cksum_golden);

	return 0;
}

static int __maybe_unused check_dsi_checksum(void)
{
	struct cmdqRecStruct *handle;
	int ret;

	if (!cksum_golden)
		return 0;

	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	if (ret) {
		DISPWARN("Fail to create cmdq handle\n");
		return -1;
	}
	if (!cksum_slot) {
		ret = cmdqBackupAllocateSlot(&cksum_slot, 1);
		if (ret) {
			DISPWARN("Fail to alloc cmd slot\n");
			cmdqRecDestroy(handle);
			return -1;
		}
	}

	cmdqRecReset(handle);
	_cmdq_insert_wait_frame_done_token_mira(handle);
	cmdqRecBackupRegisterToSlot(handle, cksum_slot, 0,
		disp_addr_convert(DISPSYS_DSI0_BASE + 0x144));
	cmdqRecFlushAsyncCallback(handle, compare_dsi_checksum, 0);
	cmdqRecDestroy(handle);
	return 0;
}

/* mutex to prevent test being called in different adb shell process */
DEFINE_MUTEX(basic_test_lock);

static int primary_display_basic_test(int layer_num, unsigned int layer_en_mask,
	int w, int h, enum DISP_FORMAT fmt, int frame_num,
	int vsync_num, int offset_x, int offset_y,
	unsigned int r, unsigned int g, unsigned int b, unsigned int a,
	int mode, unsigned int cksum)
{
	int session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0);
	unsigned int Bpp, frame, i;
	enum UNIFIED_COLOR_FMT ufmt;
	struct disp_frame_cfg_t *cfg;
	int lcm_width = primary_display_get_width();
	int lcm_height = primary_display_get_height();
	enum DISP_FORMAT out_fmt = DISP_FORMAT_RGB888;
	size_t size, test_size;
	struct test_buf_info buf_info[PRIMARY_SESSION_INPUT_LAYER_COUNT];
	struct test_buf_info output_buf_info;

	cksum_golden = cksum;
	ufmt = disp_fmt_to_unified_fmt(fmt);
	Bpp = UFMT_GET_bpp(ufmt) / 8;
	size = w * h * Bpp;
	mutex_lock(&basic_test_lock);

	DISPMSG(
		"%s:layer_num=%u,en=0x%x,w=%d,h=%d,fmt=%s,frame_num=%d,vsync=%d,size=%lu\n",
		__func__, layer_num, layer_en_mask,
		w, h, unified_color_fmt_name(ufmt), frame_num,
		vsync_num, (unsigned long)size);

	if (layer_num > PRIMARY_SESSION_INPUT_LAYER_COUNT)
		goto out_unlock;

	cfg = kmalloc(sizeof(*cfg), GFP_KERNEL);
	if (!cfg)
		goto out_unlock;

	/* ======prepare buffer========= */
	/* Input buffer */
	for (i = 0; i < PRIMARY_SESSION_INPUT_LAYER_COUNT; i++) {
		memset(&buf_info[i], 0, sizeof(buf_info[i]));
		test_alloc_buffer(size, &buf_info[i]);
		draw_buffer(buf_info[i].buf_va, w, h, ufmt, r, g, b, a);
	}

	/* Output buffer */
	if ((mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE ||
		mode == DISP_SESSION_DECOUPLE_MIRROR_MODE)) {
		memset(&output_buf_info, 0, sizeof(output_buf_info));
		test_size = lcm_width * lcm_height *
			UFMT_GET_Bpp(disp_fmt_to_unified_fmt(out_fmt));
		test_alloc_buffer(test_size, &output_buf_info);
	}
	/* ======prepare config info========= */
	memset(cfg, 0, sizeof(*cfg));
	cfg->session_id = session_id;
	cfg->setter = SESSION_USER_HWC;
	cfg->input_layer_num = layer_num;
	cfg->overlap_layer_num = 4;
	cfg->present_fence_idx = -1;
	for (i = 0; i < layer_num; i++) {
		cfg->input_cfg[i].layer_id = i;
		cfg->input_cfg[i].layer_enable = !!(layer_en_mask & (1 << i));
		cfg->input_cfg[i].src_base_addr = 0;
		if (disp_helper_get_option(DISP_OPT_USE_M4U))
			cfg->input_cfg[i].src_phy_addr =
				(void *)((unsigned long)buf_info[i].buf_mva);
		else
			cfg->input_cfg[i].src_phy_addr =
				(void *)((unsigned long)buf_info[i].buf_pa);
		cfg->input_cfg[i].next_buff_idx = -1;
		cfg->input_cfg[i].src_fmt = fmt;
		cfg->input_cfg[i].src_pitch = w;
		cfg->input_cfg[i].src_offset_x = 0;
		cfg->input_cfg[i].src_offset_y = 0;
		cfg->input_cfg[i].src_width = w;
		cfg->input_cfg[i].src_height = h;

		cfg->input_cfg[i].tgt_offset_x = i * offset_x;
		cfg->input_cfg[i].tgt_offset_y = i * offset_y;
		cfg->input_cfg[i].tgt_width = w;
		cfg->input_cfg[i].tgt_height = h;
		cfg->input_cfg[i].alpha_enable = 1;
		cfg->input_cfg[i].alpha = 0xff;
		cfg->input_cfg[i].security = DISP_NORMAL_BUFFER;
		cfg->input_cfg[i].ext_sel_layer = -1;
	}
	if ((mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE ||
		mode == DISP_SESSION_DECOUPLE_MIRROR_MODE)) {
		cfg->output_en = 1;
		if (disp_helper_get_option(DISP_OPT_USE_M4U))
			cfg->output_cfg.pa =
				(void *)(output_buf_info.buf_mva);
		else
			cfg->output_cfg.pa =
				(void *)((unsigned long)output_buf_info.buf_pa);
		cfg->output_cfg.fmt = out_fmt;
		cfg->output_cfg.width = lcm_width;
		cfg->output_cfg.height = lcm_height;
		cfg->output_cfg.pitch =	lcm_width;
		cfg->output_cfg.security = DISP_NORMAL_BUFFER;
		cfg->output_cfg.buff_idx = -1;
		cfg->output_cfg.interface_idx = -1;
	}

	/*========start to trigger path =============*/
	DSI_enable_checksum(DISP_MODULE_DSI0, NULL);
	for (frame = 0; frame < frame_num; frame++) {
		primary_display_switch_mode(mode, session_id, 1);
		primary_display_frame_cfg(cfg);
		for (i = 0; i < vsync_num; i++)  {
			struct disp_session_vsync_config vsync_config;

			vsync_config.session_id = session_id;
			primary_display_wait_for_vsync(&vsync_config);
		}
		check_dsi_checksum();

		if (unlikely(basic_test_cancel)) {
			DISPWARN("%s stop because fatal signal\n", __func__);
			break;
		}
	}

	/* disable all layers */
	for (i = 0; i < layer_num; i++) {
		cfg->input_cfg[i].layer_id = i;
		cfg->input_cfg[i].layer_enable = 0;
	}
	primary_display_switch_mode(DISP_SESSION_DIRECT_LINK_MODE,
		session_id, 1);
	primary_display_frame_cfg(cfg);
	msleep(100);

	for (i = 0; i < PRIMARY_SESSION_INPUT_LAYER_COUNT; i++)
		release_test_buf(&buf_info[i]);
	if ((mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE ||
		mode == DISP_SESSION_DECOUPLE_MIRROR_MODE))
		release_test_buf(&output_buf_info);
	kfree(cfg);

out_unlock:
	mutex_unlock(&basic_test_lock);

	return 0;
}

/*
 * provided by @CJ
 * disp_fake_engine_config (rd_addr, wr_add, 1, 2047, 3, 0, 0, 0, 1, 0)
 * wr_pat: 1
 * length: 2047
 * burst : 3
 * disable_rd : 0
 * disable_wr : 0
 * latency : 0
 * loop : 1
 */
static int  disp_fake_engine_config(
				unsigned int rd_add,
				unsigned int wr_add,
				unsigned int wr_pat,
				unsigned int length,
				unsigned int brust,
				unsigned int disable_rd,
				unsigned int disable_wr,
				unsigned int latency,
				unsigned int loop)
{
	primary_display_idlemgr_kick(__func__, 1);
	DISP_REG_SET_FIELD(NULL, MMSYS_CG_FLD_FAKE_ENG,
		DISP_REG_CONFIG_MMSYS_CG_CLR0, 0x01);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_DISP_FAKE_ENG0_RD_ADDR,
		rd_add);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_DISP_FAKE_ENG0_WR_ADDR,
		wr_add);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_DISP_FAKE_ENG0_CON0,
			(wr_pat<<24) | (loop<<22) | (length));
	DISP_REG_SET(NULL, DISP_REG_CONFIG_DISP_FAKE_ENG0_CON1,
			(brust<<12) | (disable_wr<<11) |
			(disable_rd<<10) | (latency));
	DISP_REG_SET(NULL, DISP_REG_CONFIG_DISP_FAKE_ENG0_EN, 3);
	DISPMSG("Fake eng start dump CG_CON0 = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
	DISPMSG("Fake eng start dump RD_ADDR = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_DISP_FAKE_ENG0_RD_ADDR));
	DISPMSG("Fake eng start dump WD_ADDR = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_DISP_FAKE_ENG0_WR_ADDR));
	DISPMSG("Fake eng start dump FAKE_CON0 = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_DISP_FAKE_ENG0_CON0));
	DISPMSG("Fake eng start dump FAKE_CON1 = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_DISP_FAKE_ENG0_CON1));
	DISPMSG("Fake eng start dump FAKE_EN = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_DISP_FAKE_ENG0_EN));
	return 0;
}

static int disp_fake_engine_stop(void)
{
	DISP_REG_SET(NULL, DISP_REG_CONFIG_DISP_FAKE_ENG0_EN, 1);

	DISP_REG_SET(NULL, DISP_REG_CONFIG_DISP_FAKE_ENG0_RST, 1);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_DISP_FAKE_ENG0_RST, 0);
	DISP_REG_SET_FIELD(NULL, MMSYS_CG_FLD_FAKE_ENG,
		DISP_REG_CONFIG_MMSYS_CG_SET0, 0x01);
	DISPMSG("Fake eng end dump CG_CON0 = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
	return 0;
}

static void do_helper_opt(const char *opt)
{
	int ret = 0;
	char option[100] = "";
	char *tmp;
	int value, i;

	/*ex: echo helper:DISP_OPT_BYPASS_OVL,0 > /d/mtkfb */
	tmp = (char *)(opt + 7);
	for (i = 0; i < 50; i++) {
		if (tmp[i] != ',' && tmp[i] != ' ')
			option[i] = tmp[i];
		else
			break;
	}
	tmp += i + 1;
	ret = sscanf(tmp, "%d\n", &value);
	if (ret != 1) {
		DISPWARN("error to parse cmd %s: %s %s ret=%d\n",
			opt, option, tmp, ret);
		return;
	}

	DISPMSG("will set option %s to %d\n", option, value);
	disp_helper_set_option_by_name(option, value);

}

#if 0
static void do_set_emi_bound_tb_opt(const char *opt)
{
	int ret = 0;
	int num, i, idx;
	int val[8] = {0};
	char *fmt;
	int size = sizeof("set_emi_bound_tb:%d") +
			strlen(",%d") * ARRAY_SIZE(val);

	fmt = kzalloc(size, GFP_KERNEL);
	if (!fmt)
		return;

	strncpy(fmt, "set_emi_bound_tb:%d", size);
	fmt[size - 1] = '\0';

	for (i = 0; i < ARRAY_SIZE(val); i++) {
		/* make fmt like: "set_dsi_cmd:%d,%d,%d\n" */
		strncat(fmt, ",%d", size - strlen(fmt) - 1);
	}
	strncat(fmt, "\n", size - strlen(fmt) - 1);

	num = sscanf(opt, fmt, &idx, &val[0], &val[1], &val[2],
		&val[3], &val[4], &val[5], &val[6], &val[7]);

	if (num < 2 || num > HRT_LEVEL_NUM + 1)
		goto done;

	ret = set_emi_bound_tb(idx, num - 1, val);

done:
	kfree(fmt);
	fmt = NULL;
}
#endif

struct completion dump_buf_comp;

static void process_dbg_opt(const char *opt)
{
	int ret;

	DISPMSG("disp debug cmd %s\n", opt);

	if (strncmp(opt, "helper", 6) == 0) {
		/*ex: echo helper:DISP_OPT_BYPASS_OVL,0 > /d/mtkfb */
		do_helper_opt(opt);
	} else if (strncmp(opt, "switch_mode:", 12) == 0) {
		int session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0);
		int sess_mode;

		ret = sscanf(opt, "switch_mode:%d\n", &sess_mode);
		if (ret != 1) {
			DISPWARN("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}

		primary_display_switch_mode(sess_mode, session_id, 1);
	} else if (strncmp(opt, "hrt_debug", 9) == 0) {
		gen_hrt_pattern();
		DISPMSG("hrt_debug\n");
	} else if (strncmp(opt, "dsi_mode:cmd", 12) == 0) {
		lcm_mode_status = 1;
		DISPMSG("switch cmd\n");
	} else if (strncmp(opt, "dsi_mode:vdo", 12) == 0) {
		DISPMSG("switch vdo\n");
		lcm_mode_status = 2;
	} else if (strncmp(opt, "clk_change:", 11) == 0) {
		char *p = (char *)opt + 11;
		unsigned int clk = 0;

		ret = kstrtouint(p, 0, &clk);
		if (ret) {
			DISPWARN("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}
		DISPCHECK("clk_change:%d\n", clk);
		primary_display_mipi_clk_change(clk);
	} else if (strncmp(opt, "dsipattern:", 11) == 0) {
		char *p = (char *)opt + 11;
		unsigned int pattern;

		ret = kstrtouint(p, 0, &pattern);
		if (ret) {
			DISPWARN("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}

		if (pattern) {
			DSI_BIST_Pattern_Test(DISP_MODULE_DSI0, NULL,
				true, pattern);
			DISPMSG("enable dsi pattern: 0x%08x\n", pattern);
		} else {
			primary_display_manual_lock();
			DSI_BIST_Pattern_Test(DISP_MODULE_DSI0, NULL, false, 0);
			primary_display_manual_unlock();
			return;
		}
	} else if (strncmp(opt, "mobile:", 7) == 0) {
		if (strncmp(opt + 7, "on", 2) == 0)
			g_mobilelog = 1;
		else if (strncmp(opt + 7, "off", 3) == 0)
			g_mobilelog = 0;
	} else if (strncmp(opt, "bypass_blank:", 13) == 0) {
		char *p = (char *)opt + 13;
		unsigned int blank;

		ret = kstrtouint(p, 0, &blank);
		if (ret) {
			DISPWARN("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}
		if (blank)
			bypass_blank = 1;
		else
			bypass_blank = 0;
	} else if (strncmp(opt, "stop_fake_eng", 13) == 0) {
		DISPMSG("STOP FAKE\n");
		disp_fake_engine_stop();
	} else if (strncmp(opt, "fake_eng:", 9) == 0) {
		DISPMSG("START FAKE, THE CMD:%s", opt+9);
		if (strncmp(opt + 9, "de", 2) == 0) {
			disp_fake_engine_config(fb_pa, fb_pa+4, 1,
				2047, 3, 0, 0, 0, 1);
		} else {
			unsigned int WR_mode = 0;
			unsigned int loop_mode = 0;
			unsigned int test_len = 0;
			unsigned int burst_len = 0;
			unsigned int latency = 0;

			ret = sscanf(opt, "fake_eng:%d,%d,%d,%d,%d\n",
				     &WR_mode, &loop_mode, &test_len,
				     &burst_len, &latency);
			if (ret != 5) {
				pr_debug("%d error to parse cmd %s\n",
					__LINE__, opt);
				return;
			}
			disp_fake_engine_config(fb_pa, fb_pa+1, 1,
				test_len, burst_len, 0, 0, latency, loop_mode);
		}
	} else if (strncmp(opt, "force_fps:", 9) == 0) {
		unsigned int keep;
		unsigned int skip;

		ret = sscanf(opt, "force_fps:%d,%d\n", &keep, &skip);
		if (ret != 2) {
			DISPWARN("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}

		DISPMSG("force set fps, keep %d, skip %d\n", keep, skip);
		primary_display_force_set_fps(keep, skip);
	} else if (strncmp(opt, "AAL_trigger", 11) == 0) {
		int i = 0;
		struct disp_session_vsync_config vsync_config;

		for (i = 0; i < 1200; i++) {
			primary_display_wait_for_vsync(&vsync_config);
			dpmgr_module_notify(DISP_MODULE_AAL0,
				DISP_PATH_EVENT_TRIGGER);
		}
#ifdef CONFIG_MTK_DISPLAY_120HZ_SUPPORT
	} else if (strncmp(opt, "odbypass:", 9) == 0) {
		char *p = (char *)opt + 9;
		int bypass = kstrtoul(p, 16, (unsigned long int *)&p);

		primary_display_od_bypass(bypass);
		DISPMSG("OD bypass: %d\n", bypass);
		return;
#endif
	} else if (strncmp(opt, "diagnose", 8) == 0) {
		primary_display_diagnose(__func__, __LINE__);
		return;
	} else if (strncmp(opt, "_efuse_test", 11) == 0) {
		primary_display_check_test();
	} else if (strncmp(opt, "dprec_reset", 11) == 0) {
		dprec_logger_reset_all();
		return;
	} else if (strncmp(opt, "suspend", 7) == 0) {
		primary_display_suspend();
		return;
	} else if (strncmp(opt, "resume", 6) == 0) {
		primary_display_resume();
	} else if (strncmp(opt, "ata", 3) == 0) {
		mtkfb_fm_auto_test();
		return;
	} else if (strncmp(opt, "dalprintf", 9) == 0) {
		DAL_Printf("display aee layer test\n");
	} else if (strncmp(opt, "dalclean", 8) == 0) {
		DAL_Clean();
	} else if (strncmp(opt, "daltest", 7) == 0) {
		int i = 1000;

		while (i--) {
			DAL_Printf("display aee layer test\n");
			msleep(20);
			DAL_Clean();
			msleep(20);
		}
	} else if (strncmp(opt, "lfr_setting:", 12) == 0) {
		unsigned int enable;
		unsigned int mode;
		/* unsigned int  mode=3; */
		unsigned int type = 0;
		unsigned int skip_num = 1;

		ret = sscanf(opt, "lfr_setting:%d,%d\n", &enable, &mode);
		if (ret != 2) {
			DISPWARN("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}
		DDPMSG("--------------enable/disable lfr--------------\n");
		if (enable) {
			DDPMSG("lfr enable %d mode =%d\n", enable, mode);
			enable = 1;
			DSI_Set_LFR(DISP_MODULE_DSI0, NULL,
				mode, type, enable, skip_num);
		} else {
			DDPMSG("lfr disable %d mode=%d\n", enable, mode);
			enable = 0;
			DSI_Set_LFR(DISP_MODULE_DSI0, NULL,
				mode, type, enable, skip_num);
		}
	} else if (strncmp(opt, "vsync_switch:", 13) == 0) {
		char *p = (char *)opt + 13;
		unsigned int method = 0;

		ret = kstrtouint(p, 0, &method);
		if (ret) {
			DISPWARN("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}
		primary_display_vsync_switch(method);
	} else if (strncmp(opt, "dsi0_clk:", 9) == 0) {
		char *p = (char *)opt + 9;
		UINT32 clk;

		ret = kstrtouint(p, 0, &clk);
		if (ret) {
			DISPWARN("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}
	} else if (strncmp(opt, "dst_switch:", 11) == 0) {
		char *p = (char *)opt + 11;
		UINT32 mode;

		ret = kstrtouint(p, 0, &mode);
		if (ret) {
			DISPWARN("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}
		primary_display_switch_dst_mode(mode % 2);
		return;
	} else if (strncmp(opt, "cv_switch:", 10) == 0) {
		char *p = (char *)opt + 10;
		UINT32 mode;

		ret = kstrtouint(p, 0, &mode);
		if (ret) {
			DISPWARN("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}
		disp_helper_set_option(DISP_OPT_CV_BYSUSPEND, mode % 2);
		return;
	} else if (strncmp(opt, "cmmva_dprec", 11) == 0) {
		dprec_handle_option(0x7);
	} else if (strncmp(opt, "cmmpa_dprec", 11) == 0) {
		dprec_handle_option(0x3);
	} else if (strncmp(opt, "dprec", 5) == 0) {
		char *p = (char *)opt + 6;
		unsigned int option;

		ret = kstrtouint(p, 0, &option);
		if (ret) {
			DISPWARN("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}
		dprec_handle_option(option);
	} else if (strncmp(opt, "maxlayer", 8) == 0) {
		char *p = (char *)opt + 9;
		unsigned int maxlayer;

		ret = kstrtouint(p, 0, &maxlayer);
		if (ret) {
			DISPWARN("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}

		if (maxlayer)
			primary_display_set_max_layer(maxlayer);
		else
			DISPWARN("can't set max layer to 0\n");
	} else if (strncmp(opt, "primary_reset", 13) == 0) {
		primary_display_reset();
	} else if (strncmp(opt, "esd_check", 9) == 0) {
		char *p = (char *)opt + 10;
		unsigned int enable;

		ret = kstrtouint(p, 0, &enable);
		if (ret) {
			DISPWARN("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}
		primary_display_esd_check_enable(enable);
	} else if (strncmp(opt, "esd_recovery", 12) == 0) {
		primary_display_esd_recovery();
	} else if (strncmp(opt, "set_esd_mode:", 13) == 0) {
		char *p = (char *)opt + 13;
		unsigned int mode;

		ret = kstrtouint(p, 0, &mode);
		if (ret) {
			DISPWARN("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}
		set_esd_check_mode(mode);
	} else if (strncmp(opt, "lcm0_reset", 10) == 0) {
		DISPCHECK("lcm0_reset\n");
#if 1
		DISP_CPU_REG_SET(DISP_REG_CONFIG_MMSYS_LCM_RST_B, 1);
		msleep(20);
		DISP_CPU_REG_SET(DISP_REG_CONFIG_MMSYS_LCM_RST_B, 0);
		msleep(20);
		DISP_CPU_REG_SET(DISP_REG_CONFIG_MMSYS_LCM_RST_B, 1);
#else
#ifdef CONFIG_MTK_LEGACY
		mt_set_gpio_mode(GPIO106 | 0x80000000, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO106 | 0x80000000, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO106 | 0x80000000, GPIO_OUT_ONE);
		msleep(20);
		mt_set_gpio_out(GPIO106 | 0x80000000, GPIO_OUT_ZERO);
		msleep(20);
		mt_set_gpio_out(GPIO106 | 0x80000000, GPIO_OUT_ONE);
#else
		ret = disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);
		msleep(20);
		ret |= disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT0);
		msleep(20);
		ret |= disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);
#endif
#endif
	} else if (strncmp(opt, "lcm0_reset0", 11) == 0) {
		DISP_CPU_REG_SET(DISP_REG_CONFIG_MMSYS_LCM_RST_B, 0);
	} else if (strncmp(opt, "lcm0_reset1", 11) == 0) {
		DISP_CPU_REG_SET(DISP_REG_CONFIG_MMSYS_LCM_RST_B, 1);
	} else if (strncmp(opt, "dump_layer:", 11) == 0) {
		if (strncmp(opt + 11, "on", 2) == 0) {
			ret = sscanf(opt, "dump_layer:on,%d,%d,%d\n",
				&gCapturePriLayerDownX, &gCapturePriLayerDownY,
				&gCapturePriLayerNum);
			if (ret != 3) {
				DISPWARN("%d error to parse cmd %s\n",
					__LINE__, opt);
				return;
			}

			gCapturePriLayerEnable = 1;
			gCaptureWdmaLayerEnable = 1;
			if (gCapturePriLayerDownX == 0)
				gCapturePriLayerDownX = 20;
			if (gCapturePriLayerDownY == 0)
				gCapturePriLayerDownY = 20;
			DDPMSG("dump_layer En %d DownX %d DownY %d,Num %d",
				gCapturePriLayerEnable, gCapturePriLayerDownX,
				gCapturePriLayerDownY, gCapturePriLayerNum);

		} else if (strncmp(opt + 11, "off", 3) == 0) {
			gCapturePriLayerEnable = 0;
			gCaptureWdmaLayerEnable = 0;
			gCapturePriLayerNum = TOTAL_OVL_LAYER_NUM;
			DDPMSG("dump_layer En %d\n", gCapturePriLayerEnable);
		}
	} else if (strncmp(opt, "dump_wdma_layer:", 16) == 0) {
		if (strncmp(opt + 16, "on", 2) == 0) {
			ret = sscanf(opt, "dump_wdma_layer:on,%d,%d\n",
				&gCapturePriLayerDownX, &gCapturePriLayerDownY);
			if (ret != 2) {
				DISPWARN("%d error to parse cmd %s\n",
					__LINE__, opt);
				return;
			}

			gCaptureWdmaLayerEnable = 1;
			if (gCapturePriLayerDownX == 0)
				gCapturePriLayerDownX = 20;
			if (gCapturePriLayerDownY == 0)
				gCapturePriLayerDownY = 20;
			DDPMSG("dump_wdma_layer En %d DownX %d DownY %d",
				gCaptureWdmaLayerEnable, gCapturePriLayerDownX,
				gCapturePriLayerDownY);

		} else if (strncmp(opt + 16, "off", 3) == 0) {
			gCaptureWdmaLayerEnable = 0;
			DDPMSG("dump_layer En %d\n", gCaptureWdmaLayerEnable);
		}
	} else if (strncmp(opt, "dump_rdma_layer:", 16) == 0) {
#if defined(CONFIG_MTK_ENG_BUILD) || !defined(CONFIG_MTK_GMO_RAM_OPTIMIZE)
		if (strncmp(opt + 16, "on", 2) == 0) {
			ret = sscanf(opt, "dump_rdma_layer:on,%d,%d\n",
				     &gCapturePriLayerDownX,
				     &gCapturePriLayerDownY);
			if (ret != 2) {
				DISPWARN("%d error to parse cmd %s\n",
					__LINE__, opt);
				return;
			}

			gCaptureRdmaLayerEnable = 1;
			if (gCapturePriLayerDownX == 0)
				gCapturePriLayerDownX = 20;
			if (gCapturePriLayerDownY == 0)
				gCapturePriLayerDownY = 20;
			DDPMSG("dump_wdma_layer En %d DownX %d DownY %d",
				gCaptureRdmaLayerEnable, gCapturePriLayerDownX,
				gCapturePriLayerDownY);
		} else if (strncmp(opt + 16, "off", 3) == 0) {
			gCaptureRdmaLayerEnable = 0;
			DDPMSG("dump_layer En %d\n", gCaptureRdmaLayerEnable);
		}
#endif
	} else if (strncmp(opt, "enable_idlemgr:", 15) == 0) {
		char *p = (char *)opt + 15;
		UINT32 flg;

		ret = kstrtouint(p, 0, &flg);
		if (ret) {
			DISPWARN("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}
		enable_idlemgr(flg);
	} else if (strncmp(opt, "fps:", 4) == 0) {
		char *p = (char *)opt+4;
		int fps = kstrtoul(p, 10, (unsigned long int *)&p);

		DDPMSG("change fps\n");
		primary_display_set_lcm_refresh_rate(fps);
		return;
	} else if (strncmp(opt, "disp_mode:", 10) == 0) {
		char *p = (char *)opt + 10;
		unsigned long int disp_mode = 0;

		ret = kstrtoul(p, 10, &disp_mode);
		gTriggerDispMode = (int)disp_mode;
		if (ret)
			DISPWARN("DISP/%s: errno %d\n", __func__, ret);

		DISPMSG("DDP: gTriggerDispMode=%d\n", gTriggerDispMode);
	} else if (strncmp(opt, "disp_set_fps:", 13) == 0) {
		char *p = (char *)opt + 13;
		unsigned int disp_fps = 0;

		ret = kstrtouint(p, 0, &disp_fps);
		DDPMSG("Display debug command: disp_set_fps start\n");
		primary_display_force_set_vsync_fps(disp_fps, 0);
		DDPMSG("Display debug command: disp_set_fps done\n");
	} else if (strncmp(opt, "disp_set_max_fps", 16) == 0) {
		int fps = 0;

		DDPMSG("Display debug command: disp_set_max_fps start\n");
		fps = primary_display_get_max_refresh_rate();
		primary_display_force_set_vsync_fps(fps, 0);
		DDPMSG("Display debug command: disp_set_max_fps done\n");
	} else if (strncmp(opt, "disp_set_min_fps", 16) == 0) {
		int fps = 0;

		DDPMSG("Display debug command: disp_set_min_fps start\n");
		fps = primary_display_get_min_refresh_rate();
		primary_display_force_set_vsync_fps(fps, 0);
		DDPMSG("Display debug command: disp_set_min_fps done\n");
	} else if (strncmp(opt, "disp_enter_idle_fps", 19) == 0) {
		DDPMSG("Display debug command: disp_enter_idle_fps start\n");
		primary_display_force_set_vsync_fps(50, 1);
		DDPMSG("Display debug command: disp_enter_idle_fps done\n");
	} else if (strncmp(opt, "disp_leave_idle_fps", 19) == 0) {
		DDPMSG("Display debug command: disp_leave_idle_fps start\n");
		primary_display_force_set_vsync_fps(60, 2);
		DDPMSG("Display debug command: disp_leave_idle_fps done\n");
	} else if (strncmp(opt, "disp_get_fps", 12) == 0) {
		unsigned int disp_fps = 0;

		DDPMSG("Display debug command: disp_get_fps start\n");
		disp_fps = primary_display_force_get_vsync_fps();
		DDPMSG(
			"Display debug command: disp_get_fps done, disp_fps=%d\n",
			disp_fps);
#if 0
	} else if (strncmp(opt, "set_emi_bound_tb:", 17) == 0) {
		do_set_emi_bound_tb_opt(opt);
#endif
	} else if (strncmp(opt, "primary_basic_test:", 19) == 0) {
		unsigned int layer_num, w, h, fmt, frame_num;
		unsigned int vsync_num, x, y, r, g, b, a;
		unsigned int layer_en_mask, cksum;
		int mode;

		ret = sscanf(opt,
				"primary_basic_test:%d,0x%x,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,0x%x\n",
			     &layer_num, &layer_en_mask, &w, &h,
			     &fmt, &frame_num, &vsync_num,
			     &x, &y, &r, &g, &b, &a, &mode, &cksum);
		if (ret != 15 && ret != 1) {
			DISPWARN("error to parse cmd %s, ret=%d\n", opt, ret);
			return;
		}
		if (ret == 1 && layer_num == 0) {
			basic_test_cancel = 1;
			return;
		}
		basic_test_cancel = 0;

		if (mode == DISP_INVALID_SESSION_MODE ||
			mode >= DISP_SESSION_MODE_NUM)
			mode = DISP_SESSION_DIRECT_LINK_MODE;

		if (fmt == 0)
			fmt = DISP_FORMAT_RGBA8888;
		else if (fmt == 1)
			fmt = DISP_FORMAT_RGB888;
		else if (fmt == 2)
			fmt = DISP_FORMAT_RGB565;

		primary_display_basic_test(layer_num, layer_en_mask,
			w, h, fmt, frame_num, vsync_num,
			x, y, r, g, b, a, mode, cksum);
	}

	if (strncmp(opt, "pan_disp_test:", 13) == 0) {
		int frame_num;
		int bpp;

		ret = sscanf(opt, "pan_disp_test:%d,%d\n", &frame_num, &bpp);
		if (ret != 2) {
			DISPERR("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}

		pan_display_test(frame_num, bpp);
	}

	if (strncmp(opt, "dsi_ut:restart_vdo_mode", 23) == 0) {
		dpmgr_path_stop(primary_get_dpmgr_handle(), CMDQ_DISABLE);
		primary_display_diagnose(__func__, __LINE__);
		dpmgr_path_start(primary_get_dpmgr_handle(), CMDQ_DISABLE);
		dpmgr_path_trigger(primary_get_dpmgr_handle(),
			NULL, CMDQ_DISABLE);
	}

	if (strncmp(opt, "dsi_ut:restart_cmd_mode", 23) == 0) {
		dpmgr_path_stop(primary_get_dpmgr_handle(), CMDQ_DISABLE);
		primary_display_diagnose(__func__, __LINE__);

		dpmgr_path_start(primary_get_dpmgr_handle(), CMDQ_DISABLE);
		dpmgr_path_trigger(primary_get_dpmgr_handle(),
			NULL, CMDQ_DISABLE);
		dpmgr_path_stop(primary_get_dpmgr_handle(), CMDQ_DISABLE);
		primary_display_diagnose(__func__, __LINE__);

		dpmgr_path_start(primary_get_dpmgr_handle(), CMDQ_DISABLE);
		dpmgr_path_trigger(primary_get_dpmgr_handle(),
			NULL, CMDQ_DISABLE);
	}

	if (strncmp(opt, "scenario:", 8) == 0) {
		int scen;

		ret = sscanf(opt, "scenario:%d\n", &scen);
		if (ret != 1) {
			DISPWARN("%d error to parse cmd %s\n",
				__LINE__, opt);
			return;
		}
		primary_display_set_scenario(scen);
	}
	if (strncmp(opt, "layout_noncontinous:", 20) == 0) {
		ret = sscanf(opt, "layout_noncontinuous:%d\n",
			&layer_layout_allow_non_continuous);
		if (ret != 1) {
			DISPWARN("%d error to parse cmd %s\n",
				__LINE__, opt);
			return;
		}
	}
	if (strncmp(opt, "idle_wait:", 10) == 0) {
		ret = sscanf(opt, "idle_wait:%lld\n", &idle_check_interval);
		if (ret != 1) {
			DISPWARN("%d error to parse cmd %s\n",
				__LINE__, opt);
			return;
		}
		idle_check_interval =
			idle_check_interval < 17 ? 17 : idle_check_interval;
		DISPMSG("change idle interval to %lldms\n",
			idle_check_interval);
	}
	if (strncmp(opt, "layer_statistic:", 16) == 0) {
		ret = sscanf(opt, "layer_statistic:%d\n",
			&layer_statistic_enable);
		if (ret != 1) {
			DISPWARN("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}
		if (!layer_statistic_enable)
			disp_layer_info_statistic_reset();
	}
	if (strncmp(opt, "check_clk", 9) == 0)
		ddp_clk_check();
	if (strncmp(opt, "round_corner_offset_debug:", 26) == 0) {
		if (strncmp(opt + 26, "on", 2) == 0)
			round_corner_offset_enable = 1;
		else if (strncmp(opt + 26, "off", 3) == 0)
			round_corner_offset_enable = 0;
	} else if (strncmp(opt, "MIPI_CLK:", 9) == 0) {
		if (strncmp(opt + 9, "on", 2) == 0)
			mipi_clk_change(0, 1);
		else if (strncmp(opt + 9, "off", 3) == 0)
			mipi_clk_change(0, 0);
	}
	if (strncmp(opt, "change_mipi_date_rate:", 16) == 0) {
		int mipi_date_rate = 0;

		ret = sscanf(opt, "change_mipi_date_rate:%d\n",
			&mipi_date_rate);
		if (ret != 1) {
			DISPWARN("%d error to parse cmd %s\n",
				__LINE__, opt);
			return;
		}
		if (mipi_date_rate == 0)
			mipi_clk_change_by_data_rate(0, mipi_date_rate);
		else
			mipi_clk_change_by_data_rate(1, mipi_date_rate);

		DISPMSG("change mipi_date_rate to %dMhz\n",
			mipi_date_rate);
	} else if (strncmp(opt, "dump_output:", 12) == 0) {
		if (strncmp(opt + 12, "on", 2) == 0)
			dump_output = 1;
		else if (strncmp(opt + 12, "off", 3) == 0) {
			if (composed_buf) {
				vfree(composed_buf);
				composed_buf = NULL;
			}
			dump_output = 0;
		} else if (strncmp(opt + 12, "save", 4) == 0) {
			int w, h, bytes;
			struct file *bmp;

			if (dump_output == 0)
				dump_output = 1;
			w = disp_helper_get_option(DISP_OPT_FAKE_LCM_WIDTH);
			h = disp_helper_get_option(DISP_OPT_FAKE_LCM_HEIGHT);
			bytes = w * h * 3;
			if (composed_buf == NULL)
				composed_buf = vmalloc(bytes);
			init_completion(&dump_buf_comp);
			dump_output_comp = 1;
			wait_for_completion(&dump_buf_comp);
			bmp = filp_open("/sdcard/dump_output.bmp",
				O_CREAT | O_RDWR, 0);
			if (IS_ERR(bmp)) {
				vfree(composed_buf);
				composed_buf = NULL;
				return;
			}
			filp_close(bmp, NULL);
			bmp_adjust(composed_buf, bytes, w, h);
			save_bmp("/sdcard/dump_output.bmp", composed_buf, w, h);
		} else
			DISPERR("error to parse cmd %s\n", opt);
	}

#ifdef CONFIG_MTK_ENG_BUILD
	if (strncmp(opt, "rdma_threshold:", 15) == 0) {
		ret = sscanf(opt, "rdma_threshold:%d,%d,%d,%d,%d\n",
			&dbg_ultlow, &dbg_ulthigh, &dbg_prehigh,
			&dbg_urg_low, &dbg_urg_high);
		if (ret != 5) {
			DISPWARN("%d error to parse cmd %s\n", __LINE__, opt);
			return;
		}
	}
 #endif

}

static void process_dbg_cmd(char *cmd)
{
	char *tok;

	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DBG", "[mtkfb_dbg] %s\n", cmd);

	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

int debug_get_info(unsigned char *stringbuf, int buf_len)
{
	int n = 0;

	DISPFUNC();

	n += mtkfb_get_debug_state(stringbuf + n,
		buf_len - n);

	n += primary_display_get_debug_state(stringbuf + n,
		buf_len - n);

	n += disp_sync_get_debug_info(stringbuf + n,
		buf_len - n);

	n += dprec_logger_get_result_string_all(stringbuf + n,
		buf_len - n);

	n += disp_helper_get_option_list(stringbuf + n,
		buf_len - n);

	n += dprec_logger_get_buf(DPREC_LOGGER_ERROR,
		stringbuf + n, buf_len - n);

	n += dprec_logger_get_buf(DPREC_LOGGER_FENCE,
		stringbuf + n, buf_len - n);

	n += dprec_logger_get_buf(DPREC_LOGGER_DUMP,
		stringbuf + n, buf_len - n);

	n += dprec_logger_get_buf(DPREC_LOGGER_DEBUG,
		stringbuf + n, buf_len - n);

	n += dprec_logger_get_buf(DPREC_LOGGER_STATUS,
		stringbuf + n, buf_len - n);

	stringbuf[n++] = 0;
	return n;
}

void debug_info_dump_to_printk(char *buf, int buf_len)
{
	int i = 0;
	int n = buf_len;

	for (i = 0; i < n; i += 256)
		DISPMSG("%s", buf + i);
}

static ssize_t debug_read(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	int debug_bufmax;
	static int n;

	/* Debugfs read only fetch 4096 byte each time,
	 * thus whole ringbuffer need massive iteration.
	 * We only copy ringbuffer content to debugfs
	 * buffer at first time (*ppos = 0)
	 */
	if (*ppos != 0 || !is_buffer_init)
		goto out;

	DISPFUNC();

	debug_bufmax = DEBUG_BUFFER_SIZE - 1;
	n = debug_get_info(debug_buffer, debug_bufmax);
	/* debug_info_dump_to_printk(); */
out:
	return simple_read_from_buffer(ubuf, count, ppos, debug_buffer, n);
}

static ssize_t debug_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = 512 - 1;
	size_t ret;
	char cmd_buffer[512];

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&cmd_buffer, ubuf, count))
		return -EFAULT;

	cmd_buffer[count] = 0;

	process_dbg_cmd(cmd_buffer);

	return ret;
}


static const struct file_operations debug_fops = {
	.read = debug_read,
	.write = debug_write,
	.open = debug_open,
};

static ssize_t kick_read(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	return simple_read_from_buffer(ubuf,
		count, ppos, get_kick_dump(), get_kick_dump_size());
}

static const struct file_operations kickidle_fops = {
	.read = kick_read,
};

static ssize_t partial_read(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	char p[10];
	int support = 0;
	struct disp_rect roi = {0, 0, 0, 0};

	if (disp_partial_is_support()) {
		if (!ddp_debug_force_roi()) {
			support = 1;
		} else {
			roi.x = ddp_debug_force_roi_x();
			roi.y = ddp_debug_force_roi_y();
			roi.width = ddp_debug_force_roi_w();
			roi.height = ddp_debug_force_roi_h();
			if (!is_equal_full_lcm(&roi))
				support = 1;
		}
	}
	snprintf(p, 10, "%d\n", support);
	return simple_read_from_buffer(ubuf, count, ppos, p, strlen(p));
}

static const struct file_operations partial_fops = {
	.read = partial_read,
};

static int idletime_set(void *data, u64 val)
{
	if (val < 33)
		val = 33;
	if (val > 1000000)
		val = 1000000;

	idle_check_interval = val;
	return 0;
}

static int idletime_get(void *data, u64 *val)
{
	*val = idle_check_interval;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(idletime_fops, idletime_get, idletime_set, "%llu\n");

static int idlevfp_set(void *data, u64 val)
{

	if (val > 4095)
		val = 4095;

	backup_vfp_for_lp_cust((unsigned int)val);
	return 0;
}

static int idlevfp_get(void *data, u64 *val)
{
	*val = (u64)get_backup_vfp();
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(idlevfp_fops, idlevfp_get, idlevfp_set, "%llu\n");

void DBG_Init(void)
{
	struct dentry *d_folder;
	struct dentry *d_file;

	mtkfb_dbgfs = debugfs_create_file("mtkfb",
		S_IFREG | 0444, NULL, (void *)0, &debug_fops);

	d_folder = debugfs_create_dir("displowpower", NULL);
	if (d_folder) {
		d_file = debugfs_create_file("kickdump",
			S_IFREG | 0444, d_folder, NULL, &kickidle_fops);
		d_file = debugfs_create_file("partial",
			S_IFREG | 0444, d_folder, NULL, &partial_fops);
		d_file = debugfs_create_file("idletime",
			S_IFREG | 0666, d_folder, NULL, &idletime_fops);
		d_file = debugfs_create_file("idlevfp",
			S_IFREG | 0666, d_folder, NULL, &idlevfp_fops);
	}
}

void DBG_Deinit(void)
{
	debugfs_remove(mtkfb_dbgfs);
}
