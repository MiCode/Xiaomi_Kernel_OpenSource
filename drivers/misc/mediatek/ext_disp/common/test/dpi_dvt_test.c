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

#define VENDOR_CHIP_DRIVER

#include "dpi_dvt_test.h"

#if defined(RDMA_DPI_PATH_SUPPORT) || defined(DPI_DVT_TEST_SUPPORT)

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/err.h>
#include <linux/switch.h>

#include <linux/file.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/io.h>

#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/types.h>
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#endif

#include "m4u.h"
#include "ddp_log.h"
#include "ddp_dump.h"
#include "ddp_info.h"
#include "ddp_dpi.h"
#include "ddp_clkmgr.h"
#include "ddp_reg.h"
#include "ddp_reg_ovl.h"
#include "ddp_dsi.h"

#include "hdmi_drv.h"

#include "ddp_dpi_ext.h"
#include "dpi_dvt_test.h"
#include "dpi_dvt_platform.h"
#include "DpDataType.h"
#include "disp_lowpower.h"
#include "extd_factory.h"
#include "emi_bwl.h"
#include <linux/hrtimer.h>
#include <mt-plat/upmu_common.h>
#define MHL_SII8348


/*
 *
	To-do list:
	1.  Open RDMA_DPI_PATH_SUPPORT or DPI_DVT_TEST_SUPPORT;
	2.  extern picture data structure and modify Makefile,
	     e.g.  extern unsigned char kara_1280x720[2764800];
	3.  Modify sii8348 driver;
 */

/********************************************/

/*****************Resource File***************/
/*#define BMP_HEADER_SIZE 54*/

/************DPI DVT Path Control****************/
struct DPI_DVT_CONTEXT DPI_DVT_Context;
bool disconnectFlag = true;
enum PATTERN {
	A256_VERTICAL_GRAY,
	A64_VERTICAL_GRAY,
	A256_HORIZONTAL_GRAY,
	A64_HORIZONTAL_GRAY,
	COLOR_BAR,
	USER_DEFINED_COLOR,
	FRAME_BORDER,
	DOT_MOREI,
	MAX_PATTERN
};
static struct HDMI_DRIVER *hdmi_drv;
static struct disp_ddp_path_config extd_dpi_params;
static struct disp_ddp_path_config extd_rdma_params;
static struct disp_ddp_path_config extd_ovl_params;
static struct golden_setting_context temp_golden;
struct cmdqRecStruct *checksum_cmdq;
cmdqBackupSlotHandle checksum_record;
cmdqBackupSlotHandle checksum_record1;
m4u_client_t *client_ldvt;
int is_timer_inited;

static int checksum[10];
/*TEST_CASE_TYPE test_type;*/
unsigned long int ldvt_data_va[4];
unsigned int ldvt_data_mva[4];
unsigned long int data_va;
unsigned long int out_va;
unsigned int data_mva;
unsigned int out_mva;
unsigned int bypass = 1;
/***************Basic Function Start******************/
#ifdef DPI_DVT_TEST_SUPPORT
enum RDMA_MODE {
	RDMA_MODE_DIRECT_LINK = 0,
	RDMA_MODE_MEMORY = 1,
};

enum DSI_MODE {
	DSI_CMD_MODE_0 = 0,
	DSI_VDO_MODE_SYNC_PLUSE,
	DSI_VDO_MODE_SYNC_EVENT,
	DSI_VDO_MODE_BURST,
	DSI_MODE_MAX
};

enum ACTION {
	INIT,
	POWER_ON,
	POWER_OFF,
	CONFIG,
	TRIGGER,
	BYPASS,
	START,
	STOP,
	DEINIT,
	RESET,
	MAX_ACTION
};

enum PATH_TYPE {
	LONG_PATH0,
	LONG_PATH1,
	SHORT_PATH1,
	SHORT_PATH2,
	WDMA1_PATH,
	LONG_PATH1_DSI1,
	MAX_PATH
};

int dvt_module_action(enum PATH_TYPE test_path, enum ACTION action)
{
	int mod_num = 0;
	int i = 0;
	unsigned int *module = NULL;
	int cur_mod = 0;

	switch (test_path) {
	case LONG_PATH1:
		mod_num = LONG_PATH1_MODULE_NUM;
		module = long_path1_module;
		break;
	case SHORT_PATH1:
		mod_num = SHORT_PATH_MODULE_NUM;
		module = short_path_module;
		break;
	default:
		break;
	}
	switch (action) {
	case INIT:
		for (i = 0; i < mod_num; i++) {
			cur_mod = module[i];
			if (ddp_get_module_driver(cur_mod)->init != 0)
				ddp_get_module_driver(cur_mod)->init(cur_mod,
								     NULL);
		}
		break;
	case POWER_ON:
		for (i = 0; i < mod_num; i++) {
			cur_mod = module[i];
			if (ddp_get_module_driver(cur_mod)->power_on != 0)
				ddp_get_module_driver(cur_mod)->
				    power_on(cur_mod, NULL);
		}
		break;
	case POWER_OFF:
		for (i = 0; i < mod_num; i++) {
			cur_mod = module[i];
			if (ddp_get_module_driver(cur_mod)->power_off != 0)
				ddp_get_module_driver(cur_mod)->
				    power_off(cur_mod, NULL);
		}
		break;
	case START:
		for (i = 0; i < mod_num; i++) {
			cur_mod = module[i];
			if (ddp_get_module_driver(cur_mod)->start != 0)
				ddp_get_module_driver(cur_mod)->start(cur_mod,
								      NULL);
		}
		break;
	case STOP:
		for (i = 0; i < mod_num; i++) {
			cur_mod = module[i];
			if (ddp_get_module_driver(cur_mod)->stop != 0)
				ddp_get_module_driver(cur_mod)->stop(cur_mod,
								     NULL);
		}
		break;
	case RESET:
		for (i = 0; i < mod_num; i++) {
			cur_mod = module[i];
			if (ddp_get_module_driver(cur_mod)->reset != 0)
				ddp_get_module_driver(cur_mod)->reset(cur_mod,
								      NULL);
		}
		break;
	case BYPASS:
		for (i = 0; i < mod_num; i++) {
			cur_mod = module[i];
			if (ddp_get_module_driver(cur_mod)->bypass != 0)
				ddp_get_module_driver(cur_mod)->bypass(cur_mod,
								       1);
		}
		break;
	case TRIGGER:
		for (i = 0; i < mod_num; i++) {
			cur_mod = module[i];
			if (ddp_get_module_driver(cur_mod)->trigger != 0)
				ddp_get_module_driver(cur_mod)->trigger(cur_mod,
									NULL);
		}
		break;
	case DEINIT:
		for (i = 0; i < mod_num; i++) {
			cur_mod = module[i];
			if (ddp_get_module_driver(cur_mod)->deinit != 0)
				ddp_get_module_driver(cur_mod)->deinit(cur_mod,
								       NULL);
		}
		break;
	default:
		break;
	}
	return 0;
}

int dvt_init_OVL_param(unsigned int mode)
{
	memset(&extd_ovl_params, 0, sizeof(extd_ovl_params));
	if (mode == RDMA_MODE_DIRECT_LINK) {
		extd_ovl_params.ovl_layer_scanned = 0;
		extd_ovl_params.dst_dirty = 1;
		extd_ovl_params.dst_w = extd_dpi_params.dispif_config.dpi.width;
		extd_ovl_params.dst_h =
		    extd_dpi_params.dispif_config.dpi.height;
		extd_ovl_params.ovl_dirty = 1;

		extd_ovl_params.ovl_config[0].layer_en = 1;
		extd_ovl_params.ovl_config[1].layer_en = 0;
		extd_ovl_params.ovl_config[2].layer_en = 0;
		extd_ovl_params.ovl_config[3].layer_en = 0;

		extd_ovl_params.ovl_config[0].source = OVL_LAYER_SOURCE_MEM;
		extd_ovl_params.ovl_config[0].layer = 0;
		extd_ovl_params.ovl_config[0].fmt = UFMT_BGR888;
		extd_ovl_params.ovl_config[0].addr = (unsigned long)data_mva;
		extd_ovl_params.ovl_config[0].vaddr = data_va;

		extd_ovl_params.ovl_config[0].src_x = 0;
		extd_ovl_params.ovl_config[0].src_y = 0;
		extd_ovl_params.ovl_config[0].src_w =
		    extd_dpi_params.dispif_config.dpi.width;
		extd_ovl_params.ovl_config[0].src_h =
		    extd_dpi_params.dispif_config.dpi.height;
		extd_ovl_params.ovl_config[0].src_pitch =
		    extd_dpi_params.dispif_config.dpi.width * 3;

		extd_ovl_params.ovl_config[0].dst_x = 0;
		extd_ovl_params.ovl_config[0].dst_y = 0;
		extd_ovl_params.ovl_config[0].dst_w =
		    extd_dpi_params.dispif_config.dpi.width;
		extd_ovl_params.ovl_config[0].dst_h =
		    extd_dpi_params.dispif_config.dpi.height;
		extd_ovl_params.ovl_config[0].ext_sel_layer = -1;

		extd_ovl_params.ovl_config[0].isDirty = 1;

	}

	return 0;
}

#if 0

int dsi_init_OVL_param(unsigned int mode)
{
	memset(&extd_ovl_params, 0, sizeof(extd_ovl_params));
	if (mode == RDMA_MODE_DIRECT_LINK) {
		extd_ovl_params.ovl_layer_scanned = 0;
		extd_ovl_params.dst_dirty = 1;
		extd_ovl_params.dst_w =
		    extd_dpi_params.dispif_config.dpi.width * 3;
		extd_ovl_params.dst_h =
		    extd_dpi_params.dispif_config.dpi.height;
		extd_ovl_params.ovl_dirty = 1;

		extd_ovl_params.ovl_config[0].layer_en = 1;
		extd_ovl_params.ovl_config[1].layer_en = 0;
		extd_ovl_params.ovl_config[2].layer_en = 0;
		extd_ovl_params.ovl_config[3].layer_en = 0;

		extd_ovl_params.ovl_config[0].source = OVL_LAYER_SOURCE_MEM;
		extd_ovl_params.ovl_config[0].layer = 0;
		extd_ovl_params.ovl_config[0].fmt = UFMT_BGR888;
		extd_ovl_params.ovl_config[0].addr = (unsigned long)data_mva;
		extd_ovl_params.ovl_config[0].vaddr = data_va;

		extd_ovl_params.ovl_config[0].src_x = 0;
		extd_ovl_params.ovl_config[0].src_y = 0;
		extd_ovl_params.ovl_config[0].src_w =
		    extd_dpi_params.dispif_config.dpi.width * 3;
		extd_ovl_params.ovl_config[0].src_h =
		    extd_dpi_params.dispif_config.dpi.height;
		extd_ovl_params.ovl_config[0].src_pitch =
		    extd_dpi_params.dispif_config.dpi.width * 3 * 3;

		extd_ovl_params.ovl_config[0].dst_x = 0;
		extd_ovl_params.ovl_config[0].dst_y = 0;
		extd_ovl_params.ovl_config[0].dst_w =
		    extd_dpi_params.dispif_config.dpi.width * 3;
		extd_ovl_params.ovl_config[0].dst_h =
		    extd_dpi_params.dispif_config.dpi.height;
		extd_ovl_params.ovl_config[0].ext_sel_layer = -1;

		extd_ovl_params.ovl_config[0].isDirty = 1;

	}

	return 0;
}

int ldvt_init_OVL_param(unsigned int mode)
{
	int i = 0;

	memset(&extd_ovl_params, 0, sizeof(extd_ovl_params));
	if (mode == RDMA_MODE_DIRECT_LINK) {
		extd_ovl_params.ovl_layer_scanned = 0;
		extd_ovl_params.dst_dirty = 1;
		extd_ovl_params.dst_w =
		    extd_dpi_params.dispif_config.dpi.width * 3;
		extd_ovl_params.dst_h =
		    extd_dpi_params.dispif_config.dpi.height;
		extd_ovl_params.ovl_dirty = 1;

		for (i = 0; i < 3; i++) {
			extd_ovl_params.ovl_config[i].layer_en = 1;

			extd_ovl_params.ovl_config[i].source =
			    OVL_LAYER_SOURCE_MEM;
			extd_ovl_params.ovl_config[i].layer = i;
			extd_ovl_params.ovl_config[i].fmt = UFMT_BGR888;
			extd_ovl_params.ovl_config[i].addr =
			    (unsigned long)ldvt_data_mva[i];
			extd_ovl_params.ovl_config[i].vaddr = ldvt_data_va[i];

			extd_ovl_params.ovl_config[i].src_x = 0;
			extd_ovl_params.ovl_config[i].src_y = 0;
			extd_ovl_params.ovl_config[i].src_w =
			    extd_dpi_params.dispif_config.dpi.width * 3;
			extd_ovl_params.ovl_config[i].src_h =
			    extd_dpi_params.dispif_config.dpi.height;
			extd_ovl_params.ovl_config[i].src_pitch =
			    extd_dpi_params.dispif_config.dpi.width * 3 * 3;

			extd_ovl_params.ovl_config[i].dst_x = 0;
			extd_ovl_params.ovl_config[i].dst_y = 0;
			extd_ovl_params.ovl_config[i].dst_w =
			    extd_dpi_params.dispif_config.dpi.width * 3;
			extd_ovl_params.ovl_config[i].dst_h =
			    extd_dpi_params.dispif_config.dpi.height;
			extd_ovl_params.ovl_config[i].ext_sel_layer = -1;

			extd_ovl_params.ovl_config[i].isDirty = 1;
			if (i == 3) {
				extd_ovl_params.ovl_config[i].aen = 1;
				extd_ovl_params.ovl_config[i].alpha = 0xff;
			} else {
				extd_ovl_params.ovl_config[i].aen = 1;
				extd_ovl_params.ovl_config[i].alpha = 0x80;
			}
		}

		extd_ovl_params.ovl_config[3].layer_en = 1;

		extd_ovl_params.ovl_config[3].source = OVL_LAYER_SOURCE_MEM;
		extd_ovl_params.ovl_config[3].layer = 3;
		extd_ovl_params.ovl_config[3].fmt = UFMT_UYVY;
		extd_ovl_params.ovl_config[3].addr =
		    (unsigned long)ldvt_data_mva[3];
		extd_ovl_params.ovl_config[3].vaddr = ldvt_data_va[3];

		extd_ovl_params.ovl_config[3].src_x = 0;
		extd_ovl_params.ovl_config[3].src_y = 0;
		extd_ovl_params.ovl_config[3].src_w =
		    extd_dpi_params.dispif_config.dpi.width * 3;
		extd_ovl_params.ovl_config[3].src_h =
		    extd_dpi_params.dispif_config.dpi.height;
		extd_ovl_params.ovl_config[3].src_pitch =
		    extd_dpi_params.dispif_config.dpi.width * 3 * 2;

		extd_ovl_params.ovl_config[3].dst_x = 0;
		extd_ovl_params.ovl_config[3].dst_y = 0;
		extd_ovl_params.ovl_config[3].dst_w =
		    extd_dpi_params.dispif_config.dpi.width * 3;
		extd_ovl_params.ovl_config[3].dst_h =
		    extd_dpi_params.dispif_config.dpi.height;

		extd_ovl_params.ovl_config[3].isDirty = 1;
		extd_ovl_params.ovl_config[3].aen = 1;
		extd_ovl_params.ovl_config[3].alpha = 0x8f;
		extd_ovl_params.ovl_config[3].ext_sel_layer = -1;
	}

	return 0;
}


int dvt_init_GAMMA_param(unsigned int mode)
{
	if (mode == RDMA_MODE_DIRECT_LINK) {
		extd_gamma_params.dst_dirty = 1;
		extd_gamma_params.dst_w =
		    extd_dpi_params.dispif_config.dpi.width * 3;
		extd_gamma_params.dst_h =
		    extd_dpi_params.dispif_config.dpi.height;
		extd_gamma_params.lcm_bpp = 24;
	}

	return 0;
}

int dvt_init_DSC_param(unsigned char arg)
{
	switch (arg) {
	case HDMI_VIDEO_2160p_DSC_24Hz:
	case HDMI_VIDEO_2160p_DSC_30Hz:
		{
			memset(&(extd_dpi_params.dispif_config.dpi.dsc_params),
			       0, sizeof(LCM_DSC_CONFIG_PARAMS));
			extd_dpi_params.dispif_config.dpi.dsc_enable = 1;
			/* width/(slice_mode's slice) */
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    slice_width = 1920;
			/* 32  8 */
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    slice_hight = 8;
			/* 128: 1/3 compress; 192: 1/2 compress */
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    bit_per_pixel = 128;
			/* 0: 1 slice; 1: 2 slice; 2: 3 slice */
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    slice_mode = 1;
			extd_dpi_params.dispif_config.dpi.dsc_params.rgb_swap =
			    0;
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    xmit_delay = 0x200;
			extd_dpi_params.dispif_config.dpi.dsc_params.dec_delay =
			    0x4c0;
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    scale_value = 0x20;
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    increment_interval = 0x11e;
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    decrement_interval = 0x1a;
			extd_dpi_params.dispif_config.dpi.dsc_params.
				nfl_bpg_offset = 0xdb7;
			extd_dpi_params.dispif_config.dpi.dsc_params.
				slice_bpg_offset = 0x394;
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    final_offset = 0x10f0;
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    line_bpg_offset = 0xc;
			extd_dpi_params.dispif_config.dpi.dsc_params.bp_enable =
			    0x0;
			extd_dpi_params.dispif_config.dpi.dsc_params.rct_on =
			    0x0;

			extd_dpi_params.dst_w = extd_dpi_params.
				dispif_config.dpi.width * 3;
			extd_dpi_params.dst_h = extd_dpi_params.
				dispif_config.dpi.height;

			break;
		}
	case HDMI_VIDEO_1080p_DSC_60Hz:
		{
			memset(&(extd_dpi_params.dispif_config.dpi.dsc_params),
			       0, sizeof(LCM_DSC_CONFIG_PARAMS));
			extd_dpi_params.dispif_config.dpi.dsc_enable = 1;
			/* width/(slice_mode's slice) */
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    slice_width = 960;
			/* 32  8 */
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    slice_hight = 8;
			/* 128: 1/3 compress; 192: 1/2 compress */
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    bit_per_pixel = 128;
			/* 0: 1 slice; 1: 2 slice; 2: 3 slice */
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    slice_mode = 1;
			extd_dpi_params.dispif_config.dpi.dsc_params.rgb_swap =
			    0;
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    xmit_delay = 0x200;
			extd_dpi_params.dispif_config.dpi.dsc_params.dec_delay =
			    0x2e0;
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    scale_value = 0x20;
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    increment_interval = 0xed;
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    decrement_interval = 0xd;
			extd_dpi_params.dispif_config.dpi.dsc_params.
				nfl_bpg_offset = 0xdb7;
			extd_dpi_params.dispif_config.dpi.dsc_params.
				slice_bpg_offset = 0x727;
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    final_offset = 0x10f0;
			extd_dpi_params.dispif_config.dpi.dsc_params.
			    line_bpg_offset = 0xc;
			extd_dpi_params.dispif_config.dpi.dsc_params.bp_enable =
			    0x0;
			extd_dpi_params.dispif_config.dpi.dsc_params.rct_on =
			    0x0;

			extd_dpi_params.dst_w = extd_dpi_params.
				dispif_config.dpi.width * 3;
			extd_dpi_params.dst_h = extd_dpi_params.
				dispif_config.dpi.height;

			break;
		}
	default:
		break;
	}

	return 0;
}
#endif
/*
 * OVL1 -> OVL1_MOUT -> DISP_RDMA1 -> RDMA1_SOUT -> DPI_SEL -> DPI
 */
int dvt_start_ovl1_to_dpi(unsigned int resolution, unsigned int timeS)
{
	int ret = 0;
	struct ddp_io_golden_setting_arg gset_arg;

	DPI_DVT_LOG_W("top clock on\n");
	dvt_ddp_path_top_clock_on();


	DPI_DVT_LOG_W("set Mutex and connect path\n");
	dvt_connect_ovl1_dpi(NULL);
	dvt_mutex_set_ovl1_dpi(dvt_acquire_mutex(), NULL);


	DPI_DVT_LOG_W("module init\n");
	dvt_module_action(LONG_PATH1, INIT);
	DPI_DVT_LOG_W("module init done\n");

	DPI_DVT_LOG_W("module power on\n");
	dvt_module_action(LONG_PATH1, POWER_ON);

	DPI_DVT_LOG_W("module config\n");
	ddp_driver_dpi.config(DISP_MODULE_DPI, &extd_dpi_params, NULL);
	ret = dvt_allocate_buffer(resolution, M4U_FOR_OVL1_2L);
	if (ret < 0) {
		DDPERR("dvt_allocate_buffer error: ret: %d\n", ret);
		return ret;
	}

	dvt_init_RDMA_param(RDMA_MODE_DIRECT_LINK, resolution);
	ddp_driver_rdma.config(DISP_MODULE_RDMA_LONG1, &extd_rdma_params, NULL);

	dvt_init_OVL_param(RDMA_MODE_DIRECT_LINK);
/* ddp_driver_ovl.config(DISP_MODULE_OVL1, &extd_ovl_params, NULL); */
	ddp_driver_ovl.config(DISP_MODULE_OVL1_2L, &extd_ovl_params, NULL);

	memset(&gset_arg, 0, sizeof(gset_arg));
	gset_arg.dst_mod_type = DST_MOD_REAL_TIME;
	gset_arg.is_decouple_mode = 0;
	ddp_driver_ovl.ioctl(DISP_MODULE_OVL1_2L, NULL, DDP_OVL_GOLDEN_SETTING,
			     &gset_arg);

	DPI_DVT_LOG_W("module reset\n");
	dvt_mutex_reset(NULL, dvt_acquire_mutex());
	dvt_module_action(LONG_PATH1, RESET);

	DPI_DVT_LOG_W("module start\n");
	dvt_module_action(LONG_PATH1, START);

	DPI_DVT_LOG_W("module trigger\n");
	dvt_mutex_enable(NULL, dvt_acquire_mutex());
	ddp_driver_dpi.trigger(DISP_MODULE_DPI, NULL);

	/* after trigger, we should get&release mutex */
	/* DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 1); */
	/* DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 0); */

	hdmi_drv->video_config(resolution, 0, 0);

	DPI_DVT_LOG_W("module dump_info\n");
	ddp_driver_dpi.dump_info(DISP_MODULE_DPI, 1);
	ddp_dump_analysis(DISP_MODULE_OVL1_2L);
	ddp_dump_reg(DISP_MODULE_OVL1_2L);
	ddp_dump_analysis(DISP_MODULE_RDMA_SHORT);
	ddp_dump_reg(DISP_MODULE_RDMA_SHORT);
	ddp_dump_analysis(DISP_MODULE_CONFIG);
	ddp_dump_reg(DISP_MODULE_CONFIG);
	ddp_dump_analysis(DISP_MODULE_MUTEX);
	ddp_dump_reg(DISP_MODULE_MUTEX);


	msleep(timeS * 1000);

	DPI_DVT_LOG_W("module stop\n");
	dvt_module_action(LONG_PATH1, STOP);
	dvt_mutex_disenable(NULL, dvt_acquire_mutex());


	DPI_DVT_LOG_W("module reset\n");
	dvt_mutex_reset(NULL, dvt_acquire_mutex());
	dvt_module_action(LONG_PATH1, RESET);

	DPI_DVT_LOG_W("module power off\n");
	dvt_module_action(LONG_PATH1, POWER_OFF);

	DPI_DVT_LOG_W("set Mutex and disconnect path\n");
	dvt_disconnect_ovl1_dpi(NULL);


	DPI_DVT_LOG_W("top clock off\n");
	dvt_ddp_path_top_clock_off();
	memcpy(extd_rdma_params.p_golden_setting_context, &temp_golden,
	       sizeof(temp_golden));

	if (data_va)
		vfree((void *)data_va);

	return 0;
}

/**************ROME LONG PATH End*****************/

int dpi_dvt_show_pattern(unsigned int pattern, unsigned int timeS)
{
	int val = (int)pattern;

	DPI_DVT_LOG_W("module init\n");
	ddp_driver_dpi.init(DISP_MODULE_DPI, NULL);

	DPI_DVT_LOG_W("module power on\n");
	ddp_driver_dpi.power_on(DISP_MODULE_DPI, NULL);

	ddp_driver_dpi.reset(DISP_MODULE_DPI, NULL);
	ddp_driver_dpi.config(DISP_MODULE_DPI, &extd_dpi_params, NULL);

	DISP_REG_SET_FIELD(NULL, REG_FLD(3, 4), DISPSYS_DPI_BASE + 0xF00, val);
	DISP_REG_SET_FIELD(NULL, REG_FLD(1, 0), DISPSYS_DPI_BASE + 0xF00, 1);

	if (pattern == 5) {
		DISP_REG_SET_FIELD(NULL, REG_FLD(8, 8),
				   DISPSYS_DPI_BASE + 0xF00, 0xca);
		DISP_REG_SET_FIELD(NULL, REG_FLD(8, 16),
				   DISPSYS_DPI_BASE + 0xF00, 0xbc);
		DISP_REG_SET_FIELD(NULL, REG_FLD(8, 24),
				   DISPSYS_DPI_BASE + 0xF00, 0x3e);
	}

	if (timeS == 100)
		enableRGB2YUV(acsYCbCr444);
	else if (timeS == 99) {
		configDpiRepetition();
		configInterlaceMode(HDMI_VIDEO_720x480i_60Hz);
	}

	ddp_driver_dpi.start(DISP_MODULE_DPI, NULL);
	ddp_driver_dpi.trigger(DISP_MODULE_DPI, NULL);
	msleep(50);

	hdmi_drv->video_config(DPI_DVT_Context.output_video_resolution, 0, 0);

	ddp_driver_dpi.dump_info(DISP_MODULE_DPI, 1);

	msleep(timeS * 1000);

	DPI_DisableColorBar();
	DPI_DVT_LOG_W("module stop\n");
	ddp_driver_dpi.stop(DISP_MODULE_DPI, NULL);
	ddp_driver_dpi.reset(DISP_MODULE_DPI, NULL);

	DPI_DVT_LOG_W("module power Off\n");
	ddp_driver_dpi.power_off(DISP_MODULE_DPI, NULL);

	return 0;
}

#if 0
int dvt_start_4K_rdma_to_dpi(unsigned int resolution, unsigned int timeS,
			     unsigned int caseNum)
{
	int i;
	static int loop = 15;
	int ret = 0;

	bypass = 0;


	DPI_DVT_LOG_W("top clock on\n");
	dvt_ddp_path_top_clock_on();
	/* set rdma shadow register bypass mode */
	DISP_REG_SET(NULL,
		     1 * (DDP_REG_BASE_DISP_RDMA1 - DDP_REG_BASE_DISP_RDMA0)
		     + DDP_REG_BASE_DISP_RDMA0 + DISP_REG_RDMA_SHADOW_UPDATE,
		     (0x1 << 1) | (0x1 << 2));
	DISP_REG_SET(NULL, DDP_REG_BASE_DISP_DSC + DISP_REG_DSC_SHADOW,
		     (0x1 << 1) | (0x1 << 2));

	DPI_DVT_LOG_W("set Mutex and connect path\n");
	dvt_connect_path(NULL);
	dvt_mutex_set(dvt_acquire_mutex(), NULL);

	DPI_DVT_LOG_W("module init\n");
	dvt_module_action(SHORT_PATH1, INIT);

	DPI_DVT_LOG_W("module power on\n");
	dvt_module_action(SHORT_PATH1, POWER_ON);

	if (caseNum == 0xA) {
		loop++;
		DPI_DVT_LOG_W
		    ("checksum loop %d sample checksum every %d frame\n",
		     loop % 2, (loop % 2) + 2);
		ret = dvt_allocate_buffer(resolution, M4U_FOR_RDMA1);
		dvt_init_RDMA_param(RDMA_MODE_MEMORY, resolution);

		for (i = 0; i < loop; i++)
			cmdqRecWait(checksum_cmdq,
				    CMDQ_EVENT_MUTEX1_STREAM_EOF);

		cmdqRecWait(checksum_cmdq, CMDQ_EVENT_MUTEX1_STREAM_EOF);
		cmdqRecWrite(checksum_cmdq, DPI_PHY_ADDR + 0x048, 0x80000000,
			     0x80000000);
		cmdqRecWait(checksum_cmdq, CMDQ_EVENT_MUTEX1_STREAM_EOF);
		cmdqRecPoll(checksum_cmdq, DPI_PHY_ADDR + 0x048, 0x40000000,
			    0x40000000);
		cmdqRecBackupRegisterToSlot(checksum_cmdq, checksum_record, 0,
					    DPI_PHY_ADDR + 0x048);

		for (i = 1; i < 3; i++) {
			if (loop % 2 == 1) {
/* cmdqRecWait(checksum_cmdq, CMDQ_EVENT_MUTEX1_STREAM_EOF); */
/* cmdqRecWait(checksum_cmdq, CMDQ_EVENT_MUTEX1_STREAM_EOF); */
			} else {
/* cmdqRecWait(checksum_cmdq, CMDQ_EVENT_MUTEX1_STREAM_EOF); */
			}
			cmdqRecWrite(checksum_cmdq, DPI_PHY_ADDR + 0x048,
				     0x80000000, 0x80000000);
			cmdqRecWait(checksum_cmdq,
				    CMDQ_EVENT_MUTEX1_STREAM_EOF);
			cmdqRecPoll(checksum_cmdq, DPI_PHY_ADDR + 0x048,
				    0x40000000, 0x40000000);
			cmdqRecBackupRegisterToSlot(checksum_cmdq,
						    checksum_record, i,
						    DPI_PHY_ADDR + 0x048);
		}
	}

	DPI_DVT_LOG_W("module config\n");
	ddp_driver_dpi.config(DISP_MODULE_DPI, &extd_dpi_params, NULL);

	if (caseNum != 0xA) {
		ret = dvt_allocate_buffer(resolution, M4U_FOR_RDMA1);
		if (ret < 0) {
			DDPERR("dvt_allocate_buffer error: ret: %d\n", ret);
			return ret;
		}
		dvt_init_RDMA_param(RDMA_MODE_MEMORY, resolution);
	}
	ddp_driver_rdma.config(DISP_MODULE_RDMA_SHORT, &extd_rdma_params, NULL);

	dvt_init_DSC_param(resolution);
	ddp_driver_dsc.config(DISP_MODULE_DSC, &extd_dpi_params, NULL);

	DPI_DVT_LOG_W("module reset\n");
	dvt_mutex_reset(NULL, dvt_acquire_mutex());
	dvt_module_action(SHORT_PATH1, RESET);

	DPI_DVT_LOG_W("module start\n");
	dvt_module_action(SHORT_PATH1, START);

	DPI_DVT_LOG_W("module trigger\n");
	dvt_mutex_enable(NULL, dvt_acquire_mutex());
	ddp_driver_dpi.trigger(DISP_MODULE_DPI, NULL);
	/* after trigger, we should get&release mutex */
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 1);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 0);
	mdelay(30);
	if (caseNum == 0xA)
		cmdqRecFlushAsync(checksum_cmdq);

#ifdef VENDOR_CHIP_DRIVER
	hdmi_drv->video_config(resolution, 0, 0);
#endif

	DPI_DVT_LOG_W("module dump_info\n");
/*
 *
	ddp_driver_dpi.dump_info(DISP_MODULE_DPI, 1);
	ddp_driver_rdma.dump_info(DISP_MODULE_RDMA_SHORT, 1);
	ddp_dump_analysis(DISP_MODULE_CONFIG);
	ddp_dump_reg(DISP_MODULE_CONFIG);
	ddp_dump_analysis(DISP_MODULE_MUTEX);
	ddp_dump_reg(DISP_MODULE_MUTEX);
	ddp_dump_reg(DISP_MODULE_DSC);
 */
	msleep(timeS * 1000);

	DPI_DVT_LOG_W("module stop\n");
	dvt_mutex_disenable(NULL, dvt_acquire_mutex());
	dvt_module_action(SHORT_PATH1, STOP);

	DPI_DVT_LOG_W("module reset\n");
	dvt_mutex_reset(NULL, dvt_acquire_mutex());
	dvt_module_action(SHORT_PATH1, RESET);

	DPI_DVT_LOG_W("module power off\n");
	dvt_module_action(SHORT_PATH1, POWER_OFF);

	DPI_DVT_LOG_W("set Mutex and disconnect path\n");
	dvt_disconnect_path(NULL);
	dvt_mutex_set(dvt_acquire_mutex(), NULL);

	DPI_DVT_LOG_W("top clock off\n");
	dvt_ddp_path_top_clock_off();

	if (data_va)
		vfree((void *)data_va);
	bypass = 1;
	return 0;
}

/* int init_bw_monitor_timer(void); */

int ldvt_start_4K_ovl_to_dpi(unsigned int resolution, unsigned int timeS)
{
/* int i = 0; */
	int ret = 0;
	struct ddp_io_golden_setting_arg gset_arg;
	M4U_PORT_STRUCT m4uport;

	bypass = 0;

	DPI_DVT_LOG_W("top clock on\n");
	dvt_ddp_path_top_clock_on();
	DISP_REG_SET(NULL,
		     1 * (DDP_REG_BASE_DISP_RDMA1 - DDP_REG_BASE_DISP_RDMA0)
		     + DDP_REG_BASE_DISP_RDMA0 + DISP_REG_RDMA_SHADOW_UPDATE,
		     (0x1 << 1) | (0x1 << 2));
	DISP_REG_SET(NULL, DDP_REG_BASE_DISP_DSC + DISP_REG_DSC_SHADOW,
		     (0x1 << 1) | (0x1 << 2));
	DISP_REG_SET(NULL,
		     DDP_REG_BASE_DISP_GAMMA1 - DDP_REG_BASE_DISP_GAMMA0 +
		     DISP_REG_GAMMA_DEBUG, (0x1 << 1) | (0x1 << 2));
	DISP_REG_SET(NULL,
		     DDP_REG_BASE_DISP_COLOR1 - DDP_REG_BASE_DISP_COLOR0 +
		     DISP_COLOR_SHADOW_CTRL, (0x1 << 0) | (0x1 << 2));
	DISP_REG_SET(NULL,
		     DDP_REG_BASE_DISP_CCORR1 - DDP_REG_BASE_DISP_CCORR0 +
		     DISP_REG_CCORR_SHADOW, (0x1 << 0) | (0x1 << 2));
	DISP_REG_SET(NULL,
		     DDP_REG_BASE_DISP_AAL1 - DDP_REG_BASE_DISP_AAL0 +
		     DISP_AAL_SHADOW_CTL, (0x1 << 0) | (0x1 << 2));
	DISP_REG_SET(NULL,
		     DDP_REG_BASE_DISP_DITHER1 - DDP_REG_BASE_DISP_DITHER0 +
		     DISP_REG_DITHER_0, (0x1 << 0) | (0x1 << 2));

	DISP_REG_SET_FIELD(NULL, EN_FLD_BYPASS_SHADOW,
			   DDP_REG_BASE_DISP_OVL1 + DISP_REG_OVL_EN, 0x1);
	DISP_REG_SET_FIELD(NULL, EN_FLD_RD_WRK_REG,
			   DDP_REG_BASE_DISP_OVL1 + DISP_REG_OVL_EN, 0x1);
	DISP_REG_SET_FIELD(NULL, EN_FLD_BYPASS_SHADOW,
			   DDP_REG_BASE_DISP_OVL1_2L + DISP_REG_OVL_EN, 0x1);
	DISP_REG_SET_FIELD(NULL, EN_FLD_RD_WRK_REG,
			   DDP_REG_BASE_DISP_OVL1_2L + DISP_REG_OVL_EN, 0x1);


	DPI_DVT_LOG_W("set Mutex and connect path\n");
	dvt_connect_ovl1_dpi(NULL);
	dvt_mutex_set_ovl1_dpi(dvt_acquire_mutex(), NULL);
	ddp_driver_ovl.connect(DISP_MODULE_OVL1, DISP_MODULE_UNKNOWN,
			       DISP_MODULE_OVL1_2L, 1, NULL);
	ddp_driver_ovl.connect(DISP_MODULE_OVL1_2L, DISP_MODULE_OVL1,
			       DISP_MODULE_UNKNOWN, 1, NULL);

	DPI_DVT_LOG_W("module init\n");
	dvt_module_action(LONG_PATH1, INIT);
	DPI_DVT_LOG_W("module init done\n");

	DPI_DVT_LOG_W("module power on\n");
	dvt_module_action(LONG_PATH1, POWER_ON);

	memset((void *)&m4uport, 0, sizeof(M4U_PORT_STRUCT));
	m4uport.ePortID = M4U_PORT_DISP_OVL1;
	m4uport.Virtuality = 1;
	m4uport.domain = 0;
	m4uport.Security = 0;
	m4uport.Distance = 1;
	m4uport.Direction = 0;
	m4u_config_port(&m4uport);

	DPI_DVT_LOG_W("module config\n");
	ddp_driver_dpi.config(DISP_MODULE_DPI, &extd_dpi_params, NULL);
	ret = ldvt_allocate_buffer(resolution, M4U_FOR_OVL1);
	if (ret < 0) {
		DDPERR("dvt_allocate_buffer error: ret: %d\n", ret);
		return ret;
	}

	dvt_init_GAMMA_param(RDMA_MODE_DIRECT_LINK);
	ddp_driver_gamma.config(DISP_MODULE_GAMMA1, &extd_gamma_params, NULL);
	ddp_driver_aal.config(DISP_MODULE_AAL1, &extd_gamma_params, NULL);
	ddp_driver_color.config(DISP_MODULE_COLOR1, &extd_gamma_params, NULL);
	ddp_driver_ccorr.config(DISP_MODULE_CCORR1, &extd_gamma_params, NULL);
	ddp_driver_dither.config(DISP_MODULE_DITHER1, &extd_gamma_params, NULL);
	DISP_REG_SET(NULL,
		     DISPSYS_COLOR1_BASE - DISPSYS_COLOR0_BASE +
		     DISP_COLOR_CK_ON, 0x1);

	dvt_init_DSC_param(resolution);
	ddp_driver_dsc.config(DISP_MODULE_DSC, &extd_dpi_params, NULL);

	dvt_init_RDMA_param(RDMA_MODE_DIRECT_LINK, resolution);
	ddp_driver_rdma.config(DISP_MODULE_RDMA_LONG1, &extd_rdma_params, NULL);

	ldvt_init_OVL_param(RDMA_MODE_DIRECT_LINK);
	ddp_driver_ovl.config(DISP_MODULE_OVL1, &extd_ovl_params, NULL);
	ddp_driver_ovl.config(DISP_MODULE_OVL1_2L, &extd_ovl_params, NULL);

	memset(&gset_arg, 0, sizeof(gset_arg));
	gset_arg.dst_mod_type = DST_MOD_REAL_TIME;
	gset_arg.is_decouple_mode = 0;
	ddp_driver_ovl.ioctl(DISP_MODULE_OVL1, NULL, DDP_OVL_GOLDEN_SETTING,
			     &gset_arg);
	ddp_driver_ovl.ioctl(DISP_MODULE_OVL1_2L, NULL, DDP_OVL_GOLDEN_SETTING,
			     &gset_arg);

	if (bypass)
		dvt_module_action(LONG_PATH1, BYPASS);

	DPI_DVT_LOG_W("module reset\n");
	dvt_mutex_reset(NULL, dvt_acquire_mutex());
	dvt_module_action(LONG_PATH1, RESET);

	DPI_DVT_LOG_W("module start\n");
	dvt_module_action(LONG_PATH1, START);
	DPI_DVT_LOG_W("module trigger\n");
	dvt_mutex_enable(NULL, dvt_acquire_mutex());
	ddp_driver_dpi.trigger(DISP_MODULE_DPI, NULL);

	/* after trigger, we should get&release mutex */
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 1);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 0);
	mdelay(50);
#if 0
	hdmi_drv->video_config(resolution, 0, 0);

	msleep(timeS * 1000 * 1000);


	DPI_DVT_LOG_W("module stop\n");
	dvt_mutex_disenable(NULL, dvt_acquire_mutex());
	dvt_module_action(LONG_PATH1, STOP);

	DPI_DVT_LOG_W("module reset\n");
	dvt_mutex_reset(NULL, dvt_acquire_mutex());
	dvt_module_action(LONG_PATH1, RESET);

	DPI_DVT_LOG_W("module power off\n");
	dvt_module_action(LONG_PATH1, POWER_OFF);

	DPI_DVT_LOG_W("set Mutex and disconnect path\n");
	dvt_disconnect_ovl1_dpi(NULL);
	dvt_mutex_set_ovl1_dpi(dvt_acquire_mutex(), NULL);

	DPI_DVT_LOG_W("top clock off\n");
	dvt_ddp_path_top_clock_off();
	memcpy(extd_rdma_params.p_golden_setting_context, &temp_golden,
	       sizeof(temp_golden));

	for (i = 0; i < 4; i++) {
		if (ldvt_data_va[i])
			vfree((void *)ldvt_data_va[i]);
	}
	bypass = 1;
#endif
	return 0;
}
#endif
#endif				/*#ifdef DPI_DVT_TEST_SUPPORT */
int dvt_init_WDMA_param(unsigned int resolution)
{
	memset(&extd_rdma_params, 0, sizeof(extd_rdma_params));
	extd_rdma_params.p_golden_setting_context = get_golden_setting_pgc();
	memcpy(&temp_golden, extd_rdma_params.p_golden_setting_context,
	       sizeof(temp_golden));

	extd_rdma_params.dst_dirty = 1;
	extd_rdma_params.wdma_dirty = 1;
	extd_rdma_params.dst_w = 1920;
	extd_rdma_params.dst_h = 1080;
	extd_rdma_params.wdma_config.dstAddress = (unsigned long)out_mva;
	extd_rdma_params.wdma_config.outputFormat = UFMT_RGB888;
	extd_rdma_params.wdma_config.dstPitch = 1920 * 4;
	extd_rdma_params.wdma_config.srcHeight = 1080;
	extd_rdma_params.wdma_config.srcWidth = 1920;
	extd_rdma_params.wdma_config.clipX = 0;
	extd_rdma_params.wdma_config.clipY = 0;
	extd_rdma_params.wdma_config.clipHeight = 1080;
	extd_rdma_params.wdma_config.clipWidth = 1920;
	extd_rdma_params.wdma_config.security = 0;
	extd_rdma_params.wdma_config.alpha = 0xff;
	extd_rdma_params.wdma_config.useSpecifiedAlpha = 0;

	return 0;
}

int dvt_init_RDMA_param(unsigned int mode, unsigned int resolution)
{
	memset(&extd_rdma_params, 0, sizeof(extd_rdma_params));
	extd_rdma_params.p_golden_setting_context = get_golden_setting_pgc();
	memcpy(&temp_golden, extd_rdma_params.p_golden_setting_context,
	       sizeof(temp_golden));
	if (mode == RDMA_MODE_DIRECT_LINK) {
		DPI_DVT_LOG_W("RDMA_MODE_DIRECT_LINK configure\n");
		extd_rdma_params.dst_dirty = 1;
		extd_rdma_params.rdma_dirty = 1;
		extd_rdma_params.dst_w =
		    extd_dpi_params.dispif_config.dpi.width;
		extd_rdma_params.dst_h =
		    extd_dpi_params.dispif_config.dpi.height;
		extd_rdma_params.rdma_config.address = 0;
		extd_rdma_params.rdma_config.inputFormat = 0;
		extd_rdma_params.rdma_config.pitch = 0;
		extd_rdma_params.rdma_config.dst_x = 0;
		extd_rdma_params.rdma_config.dst_y = 0;
		extd_rdma_params.lcm_bpp = 24;


	} else if (mode == RDMA_MODE_MEMORY) {
		DPI_DVT_LOG_W("RDMA_MODE_MEMORY configure\n");
		extd_rdma_params.dst_dirty = 0;
		extd_rdma_params.rdma_dirty = 1;
		extd_rdma_params.dst_w =
		    extd_dpi_params.dispif_config.dpi.width;
		extd_rdma_params.dst_h =
		    extd_dpi_params.dispif_config.dpi.height;
		extd_rdma_params.rdma_config.address = (unsigned long)data_mva;
		extd_rdma_params.rdma_config.inputFormat = UFMT_BGR888;
		extd_rdma_params.rdma_config.pitch =
		    extd_dpi_params.dispif_config.dpi.width * 3;
		extd_rdma_params.rdma_config.width =
		    extd_dpi_params.dispif_config.dpi.width;
		extd_rdma_params.rdma_config.height =
		    extd_dpi_params.dispif_config.dpi.height;
		extd_rdma_params.rdma_config.dst_x = 0;
		extd_rdma_params.rdma_config.dst_y = 0;
		extd_rdma_params.rdma_config.dst_w =
		    extd_dpi_params.dispif_config.dpi.width;
		extd_rdma_params.rdma_config.dst_h =
		    extd_dpi_params.dispif_config.dpi.height;
		extd_rdma_params.lcm_bpp = 24;

	}

	switch (resolution) {
	case HDMI_VIDEO_720x480p_60Hz:
	case HDMI_VIDEO_720x480i_60Hz:
		extd_rdma_params.p_golden_setting_context->ext_dst_height = 480;
		extd_rdma_params.p_golden_setting_context->ext_dst_width = 720;
		extd_rdma_params.p_golden_setting_context->dst_height = 480;
		extd_rdma_params.p_golden_setting_context->dst_width = 720;
		break;
	case HDMI_VIDEO_1280x720p_60Hz:
		extd_rdma_params.p_golden_setting_context->ext_dst_height = 720;
		extd_rdma_params.p_golden_setting_context->ext_dst_width = 1280;
		extd_rdma_params.p_golden_setting_context->dst_height = 720;
		extd_rdma_params.p_golden_setting_context->dst_width = 1280;
		break;
	case HDMI_VIDEO_1920x1080p_30Hz:
	case HDMI_VIDEO_1920x1080p_60Hz:
	case HDMI_VIDEO_1920x1080i_60Hz:
		extd_rdma_params.p_golden_setting_context->ext_dst_height =
		    1080;
		extd_rdma_params.p_golden_setting_context->ext_dst_width = 1920;
		extd_rdma_params.p_golden_setting_context->dst_height = 1080;
		extd_rdma_params.p_golden_setting_context->dst_width = 1920;
		break;
	case HDMI_VIDEO_2160p_DSC_24Hz:
	case HDMI_VIDEO_2160p_DSC_30Hz:
		extd_rdma_params.p_golden_setting_context->ext_dst_height =
		    2160;
		extd_rdma_params.p_golden_setting_context->ext_dst_width = 3840;
		extd_rdma_params.p_golden_setting_context->dst_height = 2160;
		extd_rdma_params.p_golden_setting_context->dst_width = 3840;
		break;
	default:
		break;
	}

	return 0;
}

void dpi_dvt_parameters(unsigned char arg)
{
	enum DPI_POLARITY clk_pol = DPI_POLARITY_RISING, de_pol =
	    DPI_POLARITY_RISING, hsync_pol = DPI_POLARITY_RISING, vsync_pol =
	    DPI_POLARITY_RISING;
	unsigned int dpi_clock = 0;
	unsigned int dpi_clk_div = 0, hsync_pulse_width = 0, hsync_back_porch =
	    0, hsync_front_porch = 0, vsync_pulse_width = 0, vsync_back_porch =
	    0, vsync_front_porch = 0;

	switch (arg) {
	case HDMI_VIDEO_720x480p_60Hz:
		{
#if defined(MHL_SII8338) || defined(MHL_SII8348)
			clk_pol = HDMI_POLARITY_FALLING;
#else
			clk_pol = HDMI_POLARITY_RISING;
#endif
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_RISING;
			vsync_pol = HDMI_POLARITY_RISING;

			dpi_clk_div = 2;

			hsync_pulse_width = 62;
			hsync_back_porch = 60;
			hsync_front_porch = 16;

			vsync_pulse_width = 6;
			vsync_back_porch = 30;
			vsync_front_porch = 9;

			DPI_DVT_Context.bg_height =
			    ((480 * DPI_DVT_Context.scaling_factor) /
			     100 >> 2) << 2;
			DPI_DVT_Context.bg_width =
			    ((720 * DPI_DVT_Context.scaling_factor) /
			     100 >> 2) << 2;
			DPI_DVT_Context.hdmi_width =
			    720 - DPI_DVT_Context.bg_width;
			DPI_DVT_Context.hdmi_height =
			    480 - DPI_DVT_Context.bg_height;
			DPI_DVT_Context.output_video_resolution =
			    HDMI_VIDEO_720x480p_60Hz;
			dpi_clock = HDMI_VIDEO_720x480p_60Hz;
			break;
		}
	case HDMI_VIDEO_720x480i_60Hz:
		{
#if defined(MHL_SII8338) || defined(MHL_SII8348)
			clk_pol = HDMI_POLARITY_FALLING;
#else
			clk_pol = HDMI_POLARITY_RISING;
#endif
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_RISING;
			vsync_pol = HDMI_POLARITY_RISING;

			dpi_clk_div = 2;

			hsync_pulse_width = 124 / 2;
			hsync_back_porch = 114 / 2;
			hsync_front_porch = 38 / 2;

			vsync_pulse_width = 6;
			vsync_back_porch = 30;
			vsync_front_porch = 9;

			DPI_DVT_Context.bg_height =
			    ((480 * DPI_DVT_Context.scaling_factor) /
			     100 >> 2) << 2;
			DPI_DVT_Context.bg_width =
			    ((1440 / 2 * DPI_DVT_Context.scaling_factor) /
			     100 >> 2) << 2;
			DPI_DVT_Context.hdmi_width =
			    1440 / 2 - DPI_DVT_Context.bg_width;
			DPI_DVT_Context.hdmi_height =
			    480 - DPI_DVT_Context.bg_height;
			DPI_DVT_Context.output_video_resolution =
			    HDMI_VIDEO_720x480i_60Hz;
			dpi_clock = 27027;
			break;
		}
	case HDMI_VIDEO_1280x720p_60Hz:
		{
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
#if defined(HDMI_TDA19989)
			hsync_pol = HDMI_POLARITY_FALLING;
#else
			hsync_pol = HDMI_POLARITY_FALLING;
#endif
			vsync_pol = HDMI_POLARITY_FALLING;

#if defined(MHL_SII8338) || defined(MHL_SII8348)
			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;
#endif
			dpi_clk_div = 2;

			hsync_pulse_width = 40;
			hsync_back_porch = 220;
			hsync_front_porch = 110;

			vsync_pulse_width = 5;
			vsync_back_porch = 20;
			vsync_front_porch = 5;
			dpi_clock = HDMI_VIDEO_1280x720p_60Hz;

			DPI_DVT_Context.bg_height =
			    ((720 * DPI_DVT_Context.scaling_factor) /
			     100 >> 2) << 2;
			DPI_DVT_Context.bg_width =
			    ((1280 * DPI_DVT_Context.scaling_factor) /
			     100 >> 2) << 2;
			DPI_DVT_Context.hdmi_width =
			    1280 - DPI_DVT_Context.bg_width;
			DPI_DVT_Context.hdmi_height =
			    720 - DPI_DVT_Context.bg_height;

			DPI_DVT_Context.output_video_resolution =
			    HDMI_VIDEO_1280x720p_60Hz;
			break;
		}
	case HDMI_VIDEO_1920x1080p_30Hz:
		{
#if defined(MHL_SII8338) || defined(MHL_SII8348)
			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;
#else
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;
#endif
			dpi_clk_div = 2;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 88;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			DPI_DVT_Context.bg_height =
			    ((1080 * DPI_DVT_Context.scaling_factor) /
			     100 >> 2) << 2;
			DPI_DVT_Context.bg_width =
			    ((1920 * DPI_DVT_Context.scaling_factor) /
			     100 >> 2) << 2;
			DPI_DVT_Context.hdmi_width =
			    1920 - DPI_DVT_Context.bg_width;
			DPI_DVT_Context.hdmi_height =
			    1080 - DPI_DVT_Context.bg_height;

			DPI_DVT_Context.output_video_resolution =
			    HDMI_VIDEO_1920x1080p_30Hz;
			dpi_clock = HDMI_VIDEO_1920x1080p_30Hz;
			break;
		}
	case HDMI_VIDEO_1920x1080p_60Hz:
		{
#if defined(MHL_SII8338) || defined(MHL_SII8348)
			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;
#else
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;
#endif
			dpi_clk_div = 2;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 88;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			DPI_DVT_Context.bg_height =
			    ((1080 * DPI_DVT_Context.scaling_factor) /
			     100 >> 2) << 2;
			DPI_DVT_Context.bg_width =
			    ((1920 * DPI_DVT_Context.scaling_factor) /
			     100 >> 2) << 2;
			DPI_DVT_Context.hdmi_width =
			    1920 - DPI_DVT_Context.bg_width;
			DPI_DVT_Context.hdmi_height =
			    1080 - DPI_DVT_Context.bg_height;

			DPI_DVT_Context.output_video_resolution =
			    HDMI_VIDEO_1920x1080p_60Hz;
			dpi_clock = HDMI_VIDEO_1920x1080p_60Hz;
			break;
		}
	case HDMI_VIDEO_1920x1080i_60Hz:
		{
#if defined(MHL_SII8338) || defined(MHL_SII8348)
			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;
#else
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;
#endif
			dpi_clk_div = 2;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 88;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			DPI_DVT_Context.bg_height =
			    ((1080 * DPI_DVT_Context.scaling_factor) /
			     100 >> 2) << 2;
			DPI_DVT_Context.bg_width =
			    ((1920 * DPI_DVT_Context.scaling_factor) /
			     100 >> 2) << 2;
			DPI_DVT_Context.hdmi_width =
			    1920 - DPI_DVT_Context.bg_width;
			DPI_DVT_Context.hdmi_height =
			    1080 - DPI_DVT_Context.bg_height;

			DPI_DVT_Context.output_video_resolution =
			    HDMI_VIDEO_1920x1080i_60Hz;
			dpi_clock = 74250;
			break;
		}

	case HDMI_VIDEO_2160p_DSC_24Hz:
		{
#if defined(MHL_SII8338) || defined(MHL_SII8348)
			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;
#else
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;
#endif
			hsync_pulse_width = 30;
			hsync_back_porch = 98;
			hsync_front_porch = 242;

			vsync_pulse_width = 10;
			vsync_back_porch = 72;
			vsync_front_porch = 8;

			DPI_DVT_Context.bg_height = 0;
			DPI_DVT_Context.bg_width = 0;
			DPI_DVT_Context.hdmi_width = 1280;
			DPI_DVT_Context.hdmi_height = 2160;

			DPI_DVT_Context.output_video_resolution =
			    HDMI_VIDEO_2160p_DSC_24Hz;
			dpi_clock = 89100;

			break;
		}
	case HDMI_VIDEO_2160p_DSC_30Hz:
		{
#if defined(MHL_SII8338) || defined(MHL_SII8348_)
/* #if defined(MHL_SII8338) || defined(MHL_SII8348) */
			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;
#else
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;
#endif
			hsync_pulse_width = 30;
			hsync_back_porch = 98;
			hsync_front_porch = 66;

			vsync_pulse_width = 10;
			vsync_back_porch = 72;
			vsync_front_porch = 8;

			DPI_DVT_Context.bg_height = 0;
			DPI_DVT_Context.bg_width = 0;
			DPI_DVT_Context.hdmi_width = 1280;
			DPI_DVT_Context.hdmi_height = 2160;

			DPI_DVT_Context.output_video_resolution =
			    HDMI_VIDEO_2160p_DSC_30Hz;
			dpi_clock = 99500;

			break;
		}

	default:
		break;
	}

	extd_dpi_params.dispif_config.dpi.width = DPI_DVT_Context.hdmi_width;
	extd_dpi_params.dispif_config.dpi.height = DPI_DVT_Context.hdmi_height;
	extd_dpi_params.dispif_config.dpi.bg_width = DPI_DVT_Context.bg_width;
	extd_dpi_params.dispif_config.dpi.bg_height = DPI_DVT_Context.bg_height;

	extd_dpi_params.dispif_config.dpi.clk_pol = clk_pol;
	extd_dpi_params.dispif_config.dpi.de_pol = de_pol;
	extd_dpi_params.dispif_config.dpi.vsync_pol = vsync_pol;
	extd_dpi_params.dispif_config.dpi.hsync_pol = hsync_pol;

	extd_dpi_params.dispif_config.dpi.hsync_pulse_width = hsync_pulse_width;
	extd_dpi_params.dispif_config.dpi.hsync_back_porch = hsync_back_porch;
	extd_dpi_params.dispif_config.dpi.hsync_front_porch = hsync_front_porch;
	extd_dpi_params.dispif_config.dpi.vsync_pulse_width = vsync_pulse_width;
	extd_dpi_params.dispif_config.dpi.vsync_back_porch = vsync_back_porch;
	extd_dpi_params.dispif_config.dpi.vsync_front_porch = vsync_front_porch;

	extd_dpi_params.dispif_config.dpi.format = 0;
	extd_dpi_params.dispif_config.dpi.rgb_order = 0;
	extd_dpi_params.dispif_config.dpi.i2x_en = true;
	extd_dpi_params.dispif_config.dpi.i2x_edge = 2;
	extd_dpi_params.dispif_config.dpi.embsync = false;
	extd_dpi_params.dispif_config.dpi.dpi_clock = dpi_clock;

	DPI_DVT_LOG_W("dpi_dvt_parameters:%d\n", arg);
}

/***********Basic Function End*********************/

void dvt_dump_ext_dpi_parameters(void)
{
	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.width:			%d\n",
	     extd_dpi_params.dispif_config.dpi.width);
	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.height:			%d\n",
	     extd_dpi_params.dispif_config.dpi.height);
	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.bg_width:		%d\n",
	     extd_dpi_params.dispif_config.dpi.bg_width);
	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.bg_height:		%d\n",
	     extd_dpi_params.dispif_config.dpi.bg_height);

	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.clk_pol:		%d\n",
	     extd_dpi_params.dispif_config.dpi.clk_pol);
	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.de_pol:			%d\n",
	     extd_dpi_params.dispif_config.dpi.de_pol);
	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.vsync_pol:		%d\n",
	     extd_dpi_params.dispif_config.dpi.vsync_pol);
	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.hsync_pol:		%d\n",
	     extd_dpi_params.dispif_config.dpi.hsync_pol);

	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.hsync_pulse_width: %d\n",
	     extd_dpi_params.dispif_config.dpi.hsync_pulse_width);
	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.hsync_back_porch:	%d\n",
	     extd_dpi_params.dispif_config.dpi.hsync_back_porch);
	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.hsync_front_porch: %d\n",
	     extd_dpi_params.dispif_config.dpi.hsync_front_porch);
	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.vsync_pulse_width: %d\n",
	     extd_dpi_params.dispif_config.dpi.vsync_pulse_width);
	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.vsync_back_porch:	%d\n",
	     extd_dpi_params.dispif_config.dpi.vsync_back_porch);
	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.vsync_front_porch: %d\n",
	     extd_dpi_params.dispif_config.dpi.vsync_front_porch);

	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.format:			%d\n",
	     extd_dpi_params.dispif_config.dpi.format);
	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.rgb_order:		%d\n",
	     extd_dpi_params.dispif_config.dpi.rgb_order);
	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.i2x_en:			%d\n",
	     extd_dpi_params.dispif_config.dpi.i2x_en);
	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.i2x_edge:		%d\n",
	     extd_dpi_params.dispif_config.dpi.i2x_edge);
	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.embsync:		%d\n",
	     extd_dpi_params.dispif_config.dpi.embsync);
	DPI_DVT_LOG_W
	    ("extd_dpi_params.dispif_config.dpi.dpi_clock:		%d\n",
	     extd_dpi_params.dispif_config.dpi.dpi_clock);
}

int dvt_copy_file_data(void *ptr, unsigned int resolution)
{
	struct file *fd = NULL;
	mm_segment_t fs;
	loff_t pos = 54;
/* int i; */
/* char *fill = NULL; */

	DPI_DVT_LOG_W("dvt_copy_file_data, resolution: %d\n", resolution);

	if (resolution == HDMI_VIDEO_1280x720p_60Hz) {
		fs = get_fs();
		set_fs(KERNEL_DS);
		fd = filp_open("/data/Gene_1280x720.bmp", O_RDONLY, 0);
		if (IS_ERR(fd)) {
			pr_info
			    ("EXTD: open Gene_1280x720.bmp fail ! ret %ld\n",
			     PTR_ERR(fd));
			set_fs(fs);
			return -1;
		}
		vfs_read(fd, ptr, 2764800, &pos);
		filp_close(fd, NULL);
		fd = NULL;
		set_fs(fs);
	} else if (resolution == HDMI_VIDEO_720x480p_60Hz) {
		fs = get_fs();
		set_fs(KERNEL_DS);
		fd = filp_open("/data/PDA0026_720x480.bmp", O_RDONLY, 0);
		if (IS_ERR(fd)) {
			pr_info
			    ("EXTD: open PDA0026_720x480.bmp fail ! ret %ld\n",
			     PTR_ERR(fd));
			set_fs(fs);
			return -1;
		}
		vfs_read(fd, ptr, 1036800, &pos);
		filp_close(fd, NULL);
		fd = NULL;
		set_fs(fs);
	} else if (resolution == HDMI_VIDEO_1920x1080p_30Hz
		   || resolution == HDMI_VIDEO_1920x1080p_60Hz) {
		fs = get_fs();
		set_fs(KERNEL_DS);
		fd = filp_open("/data/Venice_1920x1080.bmp", O_RDONLY, 0);
		if (IS_ERR(fd)) {
			pr_info
			    ("EXTD: open Venice_1920x1080.bmp fail ! ret %ld\n",
			     PTR_ERR(fd));
			set_fs(fs);
			return -1;
		}
		vfs_read(fd, ptr, 6220800, &pos);
		filp_close(fd, NULL);
		fd = NULL;
		set_fs(fs);
	} else if (resolution == HDMI_VIDEO_2160p_DSC_24Hz
		   || resolution == HDMI_VIDEO_2160p_DSC_30Hz) {
#if 0
		if (buffer_round == 3) {
			fs = get_fs();
			set_fs(KERNEL_DS);
			fd = filp_open("/data/picture_4k.yuv", O_RDONLY, 0);
			if (IS_ERR(fd)) {
				pr_info
				    ("EXTD: open picture_4k.yuv fail ! %ld\n",
				     PTR_ERR(fd));
				set_fs(fs);
				return -1;
			}
			pos = 0;
			vfs_read(fd, ptr, 16588800, &pos);
			filp_close(fd, NULL);
			fd = NULL;
			set_fs(fs);
		} else {
#endif
			fs = get_fs();
			set_fs(KERNEL_DS);
			fd = filp_open("/data/picture_4k.bmp", O_RDONLY, 0);
			if (IS_ERR(fd)) {
				pr_info
				    ("EXTD: open picture_4k.bmp fail ! %ld\n",
				     PTR_ERR(fd));
				set_fs(fs);
				return -1;
			}
			vfs_read(fd, ptr, 24883200, &pos);
			filp_close(fd, NULL);
			fd = NULL;
			set_fs(fs);
#if 0
		}
		buffer_round++;
#endif
	}
	return 0;
}

#if 0
int dvt_allocate_outbuffer(unsigned int resolution, enum HW_MODULE_Type hw_type)
{
	int ret = 0;
	M4U_PORT_STRUCT m4uport;
	m4u_client_t *client = NULL;
	int M4U_PORT = M4U_PORT_UNKNOWN;

	unsigned int hdmiPixelSize = 0;
	unsigned int hdmiDataSize = 0;
	unsigned int hdmiBufferSize = 0;

	hdmiPixelSize = 1920 * 1080;
	hdmiDataSize = hdmiPixelSize * 3;
	hdmiBufferSize = hdmiDataSize;

	DPI_DVT_LOG_W("dvt_allocate_outbuffer, width: %d, height: %d\n", 1920,
		      1080);

	out_va = (unsigned long int)vmalloc(hdmiBufferSize);
	if (((void *)out_va) == NULL) {
		DDPERR("vmalloc %d bytes fail\n", hdmiBufferSize);
		return -1;
	}
	memset((void *)out_va, 0, hdmiBufferSize);
	DPI_DVT_LOG_W("resolution %d\n", resolution);

	client = m4u_create_client();
	if (IS_ERR_OR_NULL(client)) {
		DDPERR("create client fail!\n");
		return -1;
	}

	if (hw_type == M4U_FOR_RDMA1)
		M4U_PORT = M4U_PORT_DISP_RDMA1;
#ifdef DPI_DVT_TEST_SUPPORT
	else if (hw_type == M4U_FOR_RDMA0)
		M4U_PORT = M4U_PORT_DISP_RDMA0;
	else if (hw_type == M4U_FOR_OVL0)
		M4U_PORT = M4U_PORT_DISP_OVL0;
	else if (hw_type == M4U_FOR_OVL1)
		M4U_PORT = M4U_PORT_DISP_OVL1;
	else if (hw_type == M4U_FOR_OVL1_2L)
		M4U_PORT = M4U_PORT_DISP_2L_OVL1;
	else if (hw_type == M4U_FOR_OVL0_2L)
		M4U_PORT = M4U_PORT_DISP_2L_OVL0_LARB1;
	else if (hw_type == M4U_FOR_WDMA1)
		M4U_PORT = M4U_PORT_DISP_WDMA1;
#endif

	DPI_DVT_LOG_W("data_va %lx , client %p\n", out_va, client);

	memset((void *)&m4uport, 0, sizeof(M4U_PORT_STRUCT));
	m4uport.ePortID = M4U_PORT;
	m4uport.Virtuality = 1;
	m4uport.domain = 0;
	m4uport.Security = 0;
	m4uport.Distance = 1;
	m4uport.Direction = 0;
	m4u_config_port(&m4uport);

	m4u_alloc_mva(client, M4U_PORT, (unsigned long int)out_va, NULL,
		      hdmiBufferSize, M4U_PROT_READ | M4U_PROT_WRITE, 0,
		      &out_mva);

	return ret;
}
#endif
int dvt_allocate_buffer(unsigned int resolution, enum HW_MODULE_Type hw_type)
{
	int ret = 0;
	M4U_PORT_STRUCT m4uport;
	m4u_client_t *client = NULL;
	int M4U_PORT = M4U_PORT_UNKNOWN;

	unsigned int hdmiPixelSize = 0;
	unsigned int hdmiDataSize = 0;
	unsigned int hdmiBufferSize = 0;

	if (resolution == HDMI_VIDEO_2160p_DSC_24Hz
	    || resolution == HDMI_VIDEO_2160p_DSC_30Hz) {
		hdmiPixelSize =
		    extd_dpi_params.dispif_config.dpi.width * 3 *
		    extd_dpi_params.dispif_config.dpi.height;

		hdmiDataSize = hdmiPixelSize * 3;
		hdmiBufferSize = hdmiDataSize;

		DPI_DVT_LOG_W("dvt_allocate_buffer, width: %d, height: %d\n",
			      extd_dpi_params.dispif_config.dpi.width * 3,
			      extd_dpi_params.dispif_config.dpi.height);
	} else {
		hdmiPixelSize =
		    extd_dpi_params.dispif_config.dpi.width *
		    extd_dpi_params.dispif_config.dpi.height;
		hdmiDataSize = hdmiPixelSize * 3;
		hdmiBufferSize = hdmiDataSize;

		DPI_DVT_LOG_W("dvt_allocate_buffer, width: %d, height: %d\n",
			      extd_dpi_params.dispif_config.dpi.width,
			      extd_dpi_params.dispif_config.dpi.height);
	}

	data_va = (unsigned long int)vmalloc(hdmiBufferSize);
	if (((void *)data_va) == NULL) {
		DDPERR("vmalloc %d bytes fail\n", hdmiBufferSize);
		return -1;
	}
	memset((void *)data_va, 0, hdmiBufferSize);
	DPI_DVT_LOG_W("resolution %d\n", resolution);
	ret = dvt_copy_file_data((void *)data_va, resolution);

	client = m4u_create_client();
	if (IS_ERR_OR_NULL(client)) {
		DDPERR("create client fail!\n");
		return -1;
	}

	if (hw_type == M4U_FOR_RDMA1)
		M4U_PORT = M4U_PORT_DISP_RDMA1;
#ifdef DPI_DVT_TEST_SUPPORT
	else if (hw_type == M4U_FOR_OVL1_2L)
		M4U_PORT = M4U_PORT_DISP_2L_OVL1_LARB0;
#endif

	DPI_DVT_LOG_W("data_va %lx , client %p\n", data_va, client);

	memset((void *)&m4uport, 0, sizeof(M4U_PORT_STRUCT));
	m4uport.ePortID = M4U_PORT;
	m4uport.Virtuality = 1;
	m4uport.domain = 0;
	m4uport.Security = 0;
	m4uport.Distance = 1;
	m4uport.Direction = 0;
	m4u_config_port(&m4uport);

	m4u_alloc_mva(client, M4U_PORT, (unsigned long int)data_va, NULL,
		      hdmiBufferSize, M4U_PROT_READ | M4U_PROT_WRITE, 0,
		      &data_mva);
	DPI_DVT_LOG_W("data_mva %x\n", data_mva);
/*
 *
	memset((void *)&m4uport, 0, sizeof(M4U_PORT_STRUCT));
	m4uport.ePortID = M4U_PORT_DISP_2L_OVL1_LARB0;
	m4uport.Virtuality = 1;
	m4uport.domain = 0;
	m4uport.Security = 0;
	m4uport.Distance = 1;
	m4uport.Direction = 0;
	m4u_config_port(&m4uport);
 */
	return ret;
}

#if 0
int ldvt_allocate_buffer(unsigned int resolution, enum HW_MODULE_Type hw_type)
{
	int i = 0;
	int ret = 0;
	int M4U_PORT = M4U_PORT_UNKNOWN;

	unsigned int hdmiPixelSize = 0;
	unsigned int hdmiDataSize = 0;
	unsigned int hdmiBufferSize = 0;

	if (resolution == HDMI_VIDEO_2160p_DSC_24Hz
	    || resolution == HDMI_VIDEO_2160p_DSC_30Hz) {
		hdmiPixelSize =
		    extd_dpi_params.dispif_config.dpi.width * 3 *
		    extd_dpi_params.dispif_config.dpi.height;
		hdmiDataSize = hdmiPixelSize * 3;
		hdmiBufferSize = hdmiDataSize;

		DPI_DVT_LOG_W("dvt_allocate_buffer, width: %d, height: %d\n",
			      extd_dpi_params.dispif_config.dpi.width * 3,
			      extd_dpi_params.dispif_config.dpi.height);
	} else {
		hdmiPixelSize =
		    extd_dpi_params.dispif_config.dpi.width *
		    extd_dpi_params.dispif_config.dpi.height;
		hdmiDataSize = hdmiPixelSize * 3;
		hdmiBufferSize = hdmiDataSize;

		DPI_DVT_LOG_W("dvt_allocate_buffer, width: %d, height: %d\n",
			      extd_dpi_params.dispif_config.dpi.width,
			      extd_dpi_params.dispif_config.dpi.height);
	}

	for (i = 0; i < 4; i++) {
		if (i == 3) {
			hdmiBufferSize =
			    2 * extd_dpi_params.dispif_config.dpi.width
			    * 3 * extd_dpi_params.dispif_config.dpi.height;
		}
		ldvt_data_va[i] = (unsigned long int)vmalloc(hdmiBufferSize);
		if (((void *)ldvt_data_va[i]) == NULL) {
			DDPERR("vmalloc %d bytes fail\n", hdmiBufferSize);
			return -1;
		}
		memset((void *)ldvt_data_va[i], 0, hdmiBufferSize);
		DPI_DVT_LOG_W("resolution %d\n", resolution);
		ret = dvt_copy_file_data((void *)ldvt_data_va[i], resolution);

		if (IS_ERR_OR_NULL(client_ldvt))
			client_ldvt = m4u_create_client();
		if (IS_ERR_OR_NULL(client_ldvt))
			DDPERR("create client fail!\n");
		return -1;

		DPI_DVT_LOG_W("data_va %lx , client %p\n", ldvt_data_va[i],
			      client_ldvt);

		m4u_alloc_mva(client_ldvt, M4U_PORT,
			      (unsigned long int)ldvt_data_va[i], NULL,
			      hdmiBufferSize, M4U_PROT_READ | M4U_PROT_WRITE, 0,
			      &ldvt_data_mva[i]);

		buffer_round = buffer_round % 4;
	}

	return ret;
}
#endif

int dvt_start_rdma_to_dpi(unsigned int resolution, unsigned int timeS,
			  unsigned int caseNum)
{
	int ret = 0;
	int i;

	DPI_DVT_LOG_W("top clock on\n");
	dvt_ddp_path_top_clock_on();

	DPI_DVT_LOG_W("set Mutex and connect path\n");
	dvt_connect_path(NULL);

	dvt_mutex_set(dvt_acquire_mutex(), NULL);

	DPI_DVT_LOG_W("module init\n");
	dvt_module_action(SHORT_PATH1, INIT);


	DPI_DVT_LOG_W("module power on\n");
	dvt_module_action(SHORT_PATH1, POWER_ON);

	switch (caseNum) {
	case 1:
		ret = dvt_allocate_buffer(resolution, M4U_FOR_RDMA1);
		dvt_init_RDMA_param(RDMA_MODE_MEMORY, resolution);
		/* RGB Swap: last parameter 0--RGB/1--BGR */
		DISP_REG_SET_FIELD(NULL, REG_FLD(1, 1),
				   DISPSYS_DPI_BASE + 0x010, 1);
		break;
	case 2:
		ret =
		    dvt_allocate_buffer(HDMI_VIDEO_1280x720p_60Hz,
					M4U_FOR_RDMA1);
		dvt_init_RDMA_param(RDMA_MODE_MEMORY,
				    HDMI_VIDEO_1280x720p_60Hz);
		DISP_REG_SET_FIELD(NULL, REG_FLD(1, 0),
				   DISPSYS_DPI_BASE + 0x010, 1);
		DISP_REG_SET_FIELD(NULL, REG_FLD(13, 16),
				   DISPSYS_DPI_BASE + 0x030, 0x140);
		DISP_REG_SET_FIELD(NULL, REG_FLD(13, 0),
				   DISPSYS_DPI_BASE + 0x030, 0x140);
		DISP_REG_SET_FIELD(NULL, REG_FLD(13, 16),
				   DISPSYS_DPI_BASE + 0x034, 0xB4);
		DISP_REG_SET_FIELD(NULL, REG_FLD(13, 0),
				   DISPSYS_DPI_BASE + 0x034, 0xB4);
		DISP_REG_SET_FIELD(NULL, REG_FLD(24, 0),
				   DISPSYS_DPI_BASE + 0x038, 0x3ebcca);
		break;
	case 4:
		ret = dvt_allocate_buffer(resolution, M4U_FOR_RDMA1);
		dvt_init_RDMA_param(RDMA_MODE_MEMORY, resolution);
		break;
	case 0xA:
		ret = dvt_allocate_buffer(resolution, M4U_FOR_RDMA1);
		dvt_init_RDMA_param(RDMA_MODE_MEMORY, resolution);
		cmdqRecWait(checksum_cmdq, CMDQ_EVENT_MUTEX1_STREAM_EOF);
		cmdqRecWrite(checksum_cmdq, DPI_PHY_ADDR + 0x048, 0x80000000,
			     0x80000000);
		cmdqRecWait(checksum_cmdq, CMDQ_EVENT_MUTEX1_STREAM_EOF);
		cmdqRecPoll(checksum_cmdq, DPI_PHY_ADDR + 0x048, 0x40000000,
			    0x40000000);
		cmdqRecBackupRegisterToSlot(checksum_cmdq, checksum_record, 0,
					    DPI_PHY_ADDR + 0x048);

		for (i = 1; i < 3; i++) {
			cmdqRecWrite(checksum_cmdq, DPI_PHY_ADDR + 0x048,
				     0x80000000, 0x80000000);
			cmdqRecWait(checksum_cmdq,
				    CMDQ_EVENT_MUTEX1_STREAM_EOF);
			cmdqRecPoll(checksum_cmdq, DPI_PHY_ADDR + 0x048,
				    0x40000000, 0x40000000);
			cmdqRecBackupRegisterToSlot(checksum_cmdq,
						    checksum_record, i,
						    DPI_PHY_ADDR + 0x048);
		}

		break;
	default:
		break;
	}
	DPI_DVT_LOG_W("module config\n");
	ddp_driver_dpi.config(DISP_MODULE_DPI, &extd_dpi_params, NULL);
	ddp_driver_rdma.config(DISP_MODULE_RDMA_SHORT, &extd_rdma_params, NULL);

	ddp_mutex_interrupt_enable(dvt_acquire_mutex(), NULL);


	DPI_DVT_LOG_W("module reset\n");
	dvt_mutex_reset(NULL, dvt_acquire_mutex());
	dvt_module_action(SHORT_PATH1, RESET);

	DPI_DVT_LOG_W("module start\n");
	dvt_module_action(SHORT_PATH1, START);

	DISP_REG_SET(0, DISPSYS_DPI_BASE + 0x48, 0x80000000);

	DPI_DVT_LOG_W("module trigger\n");
	dvt_mutex_enable(NULL, dvt_acquire_mutex());
	ddp_driver_dpi.trigger(DISP_MODULE_DPI, NULL);

	mdelay(30);
	if (caseNum == 0xA)
		cmdqRecFlushAsync(checksum_cmdq);

#ifdef VENDOR_CHIP_DRIVER
	hdmi_drv->video_config(resolution, 0, 0);
#endif

	DPI_DVT_LOG_W("module dump_info\n");
	ddp_driver_dpi.dump_info(DISP_MODULE_DPI, 1);
	ddp_dump_analysis(DISP_MODULE_RDMA_SHORT);
	ddp_dump_reg(DISP_MODULE_RDMA_SHORT);
	ddp_dump_analysis(DISP_MODULE_CONFIG);
	ddp_dump_reg(DISP_MODULE_CONFIG);
	ddp_dump_analysis(DISP_MODULE_MUTEX);
	ddp_dump_reg(DISP_MODULE_MUTEX);

	msleep(timeS * 1000);

	ddp_driver_dpi.dump_info(DISP_MODULE_DPI, 1);
	ddp_dump_analysis(DISP_MODULE_RDMA_SHORT);
	ddp_dump_reg(DISP_MODULE_RDMA_SHORT);
	ddp_dump_analysis(DISP_MODULE_CONFIG);
	ddp_dump_reg(DISP_MODULE_CONFIG);
	ddp_dump_analysis(DISP_MODULE_MUTEX);
	ddp_dump_reg(DISP_MODULE_MUTEX);


	DPI_DVT_LOG_W("module stop\n");

	dvt_module_action(SHORT_PATH1, STOP);

	DPI_DVT_LOG_W("module reset\n");
	dvt_module_action(SHORT_PATH1, RESET);

	DPI_DVT_LOG_W("module power off\n");
	dvt_module_action(SHORT_PATH1, POWER_OFF);

	DPI_DVT_LOG_W("set Mutex and disconnect path\n");
	dvt_disconnect_path(NULL);

	dvt_mutex_reset(NULL, dvt_acquire_mutex());
	dvt_mutex_disenable(NULL, dvt_acquire_mutex());

	DPI_DVT_LOG_W("top clock off\n");
	dvt_ddp_path_top_clock_off();

	memcpy(extd_rdma_params.p_golden_setting_context, &temp_golden,
	       sizeof(temp_golden));

	if (data_va)
		vfree((void *)data_va);

	return 0;
}


int dvt_start_rdma_to_dpi_for_RGB2YUV(unsigned int resolution,
				      unsigned int timeS,
				      enum AviColorSpace_e format)
{
	int ret = 0;

	DPI_DVT_LOG_W("top clock on\n");
	dvt_ddp_path_top_clock_on();

	DPI_DVT_LOG_W("set Mutex and connect path\n");
	dvt_connect_path(NULL);
	dvt_mutex_set(dvt_acquire_mutex(), NULL);

	DPI_DVT_LOG_W("module init\n");
	ddp_driver_dpi.init(DISP_MODULE_DPI, NULL);
	ddp_driver_rdma.init(DISP_MODULE_RDMA_SHORT, NULL);

	DPI_DVT_LOG_W("module power on\n");
	ddp_driver_dpi.power_on(DISP_MODULE_DPI, NULL);
	ddp_driver_rdma.power_on(DISP_MODULE_RDMA_SHORT, NULL);

	DPI_DVT_LOG_W("module config\n");
	ddp_driver_dpi.config(DISP_MODULE_DPI, &extd_dpi_params, NULL);
	enableRGB2YUV(format);
	ret = dvt_allocate_buffer(resolution, M4U_FOR_RDMA1);
	if (ret < 0) {
		DDPERR("dvt_allocate_buffer error: ret: %d\n", ret);
		return ret;
	}

	dvt_init_RDMA_param(RDMA_MODE_MEMORY, resolution);
	ddp_driver_rdma.config(DISP_MODULE_RDMA_SHORT, &extd_rdma_params, NULL);
	/*configRDMASwap(0, 1); */

	DPI_DVT_LOG_W("module reset\n");
	dvt_mutex_reset(NULL, dvt_acquire_mutex());
	ddp_driver_dpi.reset(DISP_MODULE_DPI, NULL);
	ddp_driver_rdma.reset(DISP_MODULE_RDMA_SHORT, NULL);

	DPI_DVT_LOG_W("module start\n");
	ddp_driver_rdma.start(DISP_MODULE_RDMA_SHORT, NULL);
	ddp_driver_dpi.start(DISP_MODULE_DPI, NULL);

	DPI_DVT_LOG_W("module trigger\n");
	dvt_mutex_enable(NULL, dvt_acquire_mutex());
	ddp_driver_dpi.trigger(DISP_MODULE_DPI, NULL);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(HW_MUTEX), 1);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(HW_MUTEX), 0);

	hdmi_drv->video_config(resolution, 0, 0);

	DPI_DVT_LOG_W("module dump_info\n");
	ddp_driver_dpi.dump_info(DISP_MODULE_DPI, 1);
	ddp_driver_rdma.dump_info(DISP_MODULE_RDMA_SHORT, 1);

	msleep(timeS * 1000);

	DPI_DVT_LOG_W("module stop\n");
	dvt_mutex_disenable(NULL, dvt_acquire_mutex());
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(HW_MUTEX), 1);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(HW_MUTEX), 0);
	ddp_driver_dpi.stop(DISP_MODULE_DPI, NULL);
	ddp_driver_rdma.stop(DISP_MODULE_RDMA_SHORT, NULL);

	DPI_DVT_LOG_W("module reset\n");
	dvt_mutex_reset(NULL, dvt_acquire_mutex());
	ddp_driver_dpi.reset(DISP_MODULE_DPI, NULL);
	ddp_driver_rdma.reset(DISP_MODULE_RDMA_SHORT, NULL);

	DPI_DVT_LOG_W("module power off\n");
	ddp_driver_dpi.power_off(DISP_MODULE_DPI, NULL);
	ddp_driver_rdma.power_off(DISP_MODULE_RDMA_SHORT, NULL);

	DPI_DVT_LOG_W("set Mutex and disconnect path\n");
	dvt_disconnect_path(NULL);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(HW_MUTEX), 1);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(HW_MUTEX), 0);

	DPI_DVT_LOG_W("top clock off\n");
	dvt_ddp_path_top_clock_off();

	if (data_va)
		vfree((void *)data_va);

	return 0;
}


/**************Test Case Start*****************/
#include "ddp_hal.h"

static int dpi_dvt_testcase_4_timing(unsigned int resolution)
{
	DPI_DVT_LOG_W("dpi_dvt_testcase_4_timing, resolution: %d\n",
		      resolution);
	/* get HDMI Driver from vendor floder */
	hdmi_drv = (struct HDMI_DRIVER *)HDMI_GetDriver();
	if (hdmi_drv == NULL) {
		DPI_DVT_LOG_W
		    ("[hdmi]%s, init fail, can't get hdmi driver handle\n",
		     __func__);
		return -1;
	}

	dpi_dvt_parameters(resolution);
	dvt_dump_ext_dpi_parameters();
	dvt_start_rdma_to_dpi(resolution, 20, 0x4);

	return 0;
}

#ifdef DPI_DVT_TEST_SUPPORT

static int dpi_dvt_testcase_2_BG(unsigned int resolution)
{
	DPI_DVT_LOG_W("dpi_dvt_testcase_2_bg, resolution: %d\n", resolution);
	/* get HDMI Driver from vendor floder */
	hdmi_drv = (struct HDMI_DRIVER *)HDMI_GetDriver();
	if (hdmi_drv == NULL) {
		DPI_DVT_LOG_W
		    ("[hdmi]%s, nit fail, can't get hdmi driver handle\n",
		     __func__);
		return -1;
	}

	dpi_dvt_parameters(resolution);
	extd_dpi_params.dispif_config.dpi.width = 1280;
	extd_dpi_params.dispif_config.dpi.height = 720;
	extd_dpi_params.dispif_config.dpi.bg_width = 640;
	extd_dpi_params.dispif_config.dpi.bg_height = 360;
	dvt_dump_ext_dpi_parameters();
	dvt_start_rdma_to_dpi(resolution, 20, 0x2);

	return 0;
}


static int dpi_dvt_testcase_3_pattern(enum PATTERN pattern)
{
	/* 0x01 -> 0x71 */
	DPI_DVT_LOG_W("dpi_dvt_testcase_3_pattern, pattern: %d\n", pattern);

	DPI_DVT_Context.output_video_resolution = HDMI_VIDEO_1280x720p_60Hz;
	dpi_dvt_parameters(DPI_DVT_Context.output_video_resolution);
	dvt_dump_ext_dpi_parameters();
	DPI_DVT_LOG_W("top clock on\n");
	dvt_ddp_path_top_clock_on();

	dpi_dvt_show_pattern(pattern, 20);
	dvt_ddp_path_top_clock_off();

	return 0;
}

static int dpi_dvt_testcase_6_yuv(unsigned int resolution,
				  enum AviColorSpace_e format)
{
	DPI_DVT_LOG_W("dpi_dvt_testcase_6_yuv444, resolution: %d, format: %d\n",
		      resolution, format);

	dpi_dvt_parameters(resolution);
	dvt_dump_ext_dpi_parameters();
	dvt_start_rdma_to_dpi_for_RGB2YUV(resolution, 30, format);

	return 0;
}

static int dpi_dvt_testcase_10_checkSum(unsigned int resolution)
{
	int i;

	DPI_DVT_LOG_W("dpi_dvt_testcase_10_checkSum, resolution: %d\n",
		      resolution);
	cmdqRecCreate(CMDQ_SCENARIO_MHL_DISP, &checksum_cmdq);
	cmdqRecReset(checksum_cmdq);
	init_cmdq_slots(&checksum_record, 3, 0);

	dpi_dvt_parameters(resolution);
	dvt_dump_ext_dpi_parameters();

	if (resolution == HDMI_VIDEO_2160p_DSC_24Hz
		|| resolution == HDMI_VIDEO_2160p_DSC_30Hz)
		;
	else
		dvt_start_rdma_to_dpi(resolution, 20, 0xA);
	for (i = 0; i < 3; i++)
		cmdqBackupReadSlot(checksum_record, i, &checksum[i]);

	DPI_DVT_LOG_W("checksum[0] = %x", checksum[0] & 0xffffff);
	for (i = 0; i < 2; i++) {
		DPI_DVT_LOG_W("checksum[%d] = %x", i, checksum[i] & 0xffffff);
		if ((checksum[i] & 0xffffff) != (checksum[i + 1] & 0xffffff))
			DPI_DVT_LOG_W
			    ("Error88888888888checksum is not the same!!\n");
	}
	DPI_DVT_LOG_W("checksum[%d] = %x", i, checksum[i] & 0xffffff);
	cmdqBackupFreeSlot(checksum_record);
	cmdqRecDestroy(checksum_cmdq);
	return 0;
}

static int dpi_dvt_testcase_11_ovl1_to_dpi(unsigned int resolution)
{
	DPI_DVT_LOG_W("dpi_dvt_testcase_11_ovl1_to_dpi, resolution: %d\n",
		      resolution);

	dpi_dvt_parameters(resolution);
	dvt_dump_ext_dpi_parameters();

	dvt_start_ovl1_to_dpi(resolution, 30);

	msleep(100);
	return 0;
}

#if 0
static int dpi_dvt_testcase_18_4K_rdma_to_dpi(unsigned int resolution)
{
	DPI_DVT_LOG_W("dpi_dvt_testcase_18_timing, resolution: %d\n",
		      resolution);

	dpi_dvt_parameters(resolution);
	dvt_dump_ext_dpi_parameters();
	dvt_start_4K_rdma_to_dpi(resolution, 20, 0);

	return 0;
}

static int ovl2mem_case19(unsigned int resolution)
{
	int ret = 0;
	char *out = NULL;
	char *data = NULL;
	unsigned int i, j = 0;
	int non_equal = 0;
	struct ddp_io_golden_setting_arg gset_arg;

	DPI_DVT_LOG_W("top clock on\n");
	dvt_ddp_path_top_clock_on();
	DISP_REG_SET(NULL,
		     DDP_REG_BASE_DISP_RDMA1 + DISP_REG_RDMA_SHADOW_UPDATE,
		     (0x1 << 1) | (0x1 << 2));
	DISP_REG_SET_FIELD(NULL, EN_FLD_BYPASS_SHADOW,
			   DDP_REG_BASE_DISP_OVL1 + DISP_REG_OVL_EN, 0x1);
	DISP_REG_SET_FIELD(NULL, EN_FLD_RD_WRK_REG,
			   DDP_REG_BASE_DISP_OVL1 + DISP_REG_OVL_EN, 0x1);
	DISP_REG_SET_FIELD(NULL, EN_FLD_BYPASS_SHADOW,
			   DDP_REG_BASE_DISP_OVL1_2L + DISP_REG_OVL_EN, 0x1);
	DISP_REG_SET_FIELD(NULL, EN_FLD_RD_WRK_REG,
			   DDP_REG_BASE_DISP_OVL1_2L + DISP_REG_OVL_EN, 0x1);

	DPI_DVT_LOG_W("set Mutex and connect path\n");
	dvt_connect_ovl1_wdma(NULL);
	dvt_mutex_set_ovl1_wdma(HW_MUTEX, NULL);

	ddp_driver_ovl.connect(DISP_MODULE_OVL1, DISP_MODULE_UNKNOWN,
			       DISP_MODULE_OVL1_2L, 1, NULL);
	ddp_driver_ovl.connect(DISP_MODULE_OVL1_2L, DISP_MODULE_OVL1,
			       DISP_MODULE_UNKNOWN, 1, NULL);
	DISP_REG_SET_FIELD(NULL, DATAPATH_CON_FLD_BGCLR_IN_SEL,
			   DDP_REG_BASE_DISP_OVL1_2L +
			   DISP_REG_OVL_DATAPATH_CON, 1);

	DPI_DVT_LOG_W("module init\n");
	dvt_module_action(WDMA1_PATH, INIT);

	DPI_DVT_LOG_W("module power on\n");
	dvt_module_action(WDMA1_PATH, POWER_ON);

	DPI_DVT_LOG_W("module config\n");
	ret = dvt_allocate_buffer(resolution, M4U_FOR_OVL1);
	if (ret < 0) {
		DDPERR("dvt_allocate_buffer error: ret: %d\n", ret);
		return ret;
	}
	ret = dvt_allocate_outbuffer(resolution, M4U_FOR_WDMA1);

	dvt_init_WDMA_param(resolution);
	ddp_driver_wdma.config(DISP_MODULE_WDMA1, &extd_rdma_params, NULL);

	memset(&gset_arg, 0, sizeof(gset_arg));
	gset_arg.dst_mod_type = DST_MOD_WDMA;
	gset_arg.is_decouple_mode = 0;

	dvt_init_OVL_param(RDMA_MODE_DIRECT_LINK);
	ddp_driver_ovl.config(DISP_MODULE_OVL1, &extd_ovl_params, NULL);
	ddp_driver_ovl.config(DISP_MODULE_OVL1_2L, &extd_ovl_params, NULL);
	/*configOVL0Layer0Swap(DISP_MODULE_OVL0, 0);    BGR:0,  RGB:1 */
	ddp_driver_ovl.ioctl(DISP_MODULE_OVL0, NULL, DDP_OVL_GOLDEN_SETTING,
			     &gset_arg);
	ddp_driver_ovl.ioctl(DISP_MODULE_OVL0_2L, NULL, DDP_OVL_GOLDEN_SETTING,
			     &gset_arg);

	DPI_DVT_LOG_W("module reset\n");
	dvt_mutex_reset(NULL, HW_MUTEX);
	dvt_module_action(WDMA1_PATH, RESET);

	DPI_DVT_LOG_W("module start\n");
	dvt_module_action(WDMA1_PATH, START);

	DPI_DVT_LOG_W("module trigger\n");
	dvt_mutex_enable(NULL, HW_MUTEX);

	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 1);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 0);
	msleep(3 * 1000);

	DPI_DVT_LOG_W("module dump_info\n");
	ddp_driver_ovl.dump_info(DISP_MODULE_OVL1_2L, 1);
	ddp_driver_wdma.dump_info(DISP_MODULE_WDMA1, 1);
	ddp_driver_ovl.dump_info(DISP_MODULE_OVL1, 0);
	ddp_dump_reg(DISP_MODULE_CONFIG);
	ddp_dump_analysis(DISP_MODULE_CONFIG);
	dvt_mutex_dump_reg();

	msleep(1 * 1000);

	DPI_DVT_LOG_W("module stop\n");
	dvt_mutex_disenable(NULL, HW_MUTEX);
	dvt_module_action(WDMA1_PATH, STOP);

	DPI_DVT_LOG_W("module reset\n");
	dvt_module_action(WDMA1_PATH, RESET);

	DPI_DVT_LOG_W("module power off\n");
	dvt_module_action(WDMA1_PATH, POWER_OFF);

	DPI_DVT_LOG_W("set Mutex and disconnect path\n");
	dvt_disconnect_ovl1_wdma(NULL);

	DPI_DVT_LOG_W("top clock off\n");
	dvt_ddp_path_top_clock_off();
	memcpy(extd_rdma_params.p_golden_setting_context, &temp_golden,
	       sizeof(temp_golden));
	out = (char *)out_va;
	data = (char *)data_va;
	for (i = 0; i < (1080 * 1920); i++) {
		for (j = 0; j < 3; j++) {
			if (out[3 * i + j] != data[3 * i + 2 - j]) {
				DPI_DVT_LOG_W("OVL1-WDMA1 TEST FAIL!\n");
				non_equal = 1;
				break;
			}
		}
		if (non_equal)
			break;
	}
	DPI_DVT_LOG_W("out_va has data %d, %d, %d\n", out[0], out[1], out[2]);
	DPI_DVT_LOG_W("data_va has data %d, %d, %d\n", data[0], data[1],
		      data[2]);

	if (data_va)
		vfree((void *)data_va);
	if (out_va)
		vfree((void *)out_va);
	return 0;
}

static int dpi_ldvt_testcase_20(unsigned int resolution)
{
	DPI_DVT_LOG_W("dpi_ldvt_testcase_20, resolution: %d\n", resolution);

	dpi_dvt_parameters(resolution);
	dvt_dump_ext_dpi_parameters();
	ldvt_start_4K_ovl_to_dpi(resolution, 20);

	return 0;
}
#endif
#endif
/* static int dpi_dsc_testcase(unsigned int resolution); */

/**********Test Case End****************/
int dpi_dvt_run_cases(unsigned int caseNum)
{
	switch (caseNum) {
	case 4:
		{

/*
 *
			dpi_dvt_testcase_4_timing(HDMI_VIDEO_1920x1080p_60Hz);
			msleep(500);
 */
			dpi_dvt_testcase_4_timing(HDMI_VIDEO_1920x1080p_30Hz);
			msleep(500);
			dpi_dvt_testcase_4_timing(HDMI_VIDEO_1280x720p_60Hz);
			msleep(500);
			dpi_dvt_testcase_4_timing(HDMI_VIDEO_720x480p_60Hz);
			msleep(500);
			break;
		}
#ifdef DPI_DVT_TEST_SUPPORT
	case 1:
		{
			dpi_dvt_parameters(HDMI_VIDEO_1920x1080p_30Hz);
			dvt_dump_ext_dpi_parameters();
			dvt_start_rdma_to_dpi(HDMI_VIDEO_1920x1080p_30Hz, 20,
					      0x1);
			break;
		}
	case 2:
		{
			dpi_dvt_testcase_2_BG(HDMI_VIDEO_1920x1080p_30Hz);
			msleep(500);
			break;
		}
	case 3:
		{
			dpi_dvt_testcase_3_pattern(0);
			msleep(500);
			dpi_dvt_testcase_3_pattern(1);
			msleep(500);
			dpi_dvt_testcase_3_pattern(2);
			msleep(500);
			dpi_dvt_testcase_3_pattern(3);
			msleep(500);
			dpi_dvt_testcase_3_pattern(4);
			msleep(500);
			dpi_dvt_testcase_3_pattern(5);
			msleep(500);
			dpi_dvt_testcase_3_pattern(6);
			msleep(500);
			dpi_dvt_testcase_3_pattern(7);
			msleep(500);
			break;
		}
	case 6:
		{
			dpi_dvt_testcase_6_yuv(HDMI_VIDEO_1920x1080p_30Hz,
					       acsYCbCr444);
			msleep(500);
			break;
		}
	case 10:
		{
/* dpi_dvt_testcase_10_checkSum(HDMI_VIDEO_2160p_DSC_30Hz); */
			dpi_dvt_testcase_10_checkSum
			    (HDMI_VIDEO_1920x1080p_30Hz);

			msleep(500);
			break;
		}
	case 11:
		{
			dpi_dvt_testcase_11_ovl1_to_dpi
			    (HDMI_VIDEO_1920x1080p_30Hz);
			msleep(500);
			dpi_dvt_testcase_11_ovl1_to_dpi
			    (HDMI_VIDEO_1280x720p_60Hz);
			msleep(500);
			dpi_dvt_testcase_11_ovl1_to_dpi
			    (HDMI_VIDEO_720x480p_60Hz);
			msleep(500);
			break;
		}
	case 18:
		{
/* dpi_dvt_testcase_18_4K_rdma_to_dpi(HDMI_VIDEO_2160p_DSC_30Hz); */
			msleep(500);
			break;
		}
	case 19:
		{
/* dpi_dvt_parameters(HDMI_VIDEO_1920x1080p_30Hz); */
/* ovl2mem_case19(HDMI_VIDEO_1920x1080p_30Hz); */
			msleep(500);
			break;
		}
	case 20:
		{
/* dpi_ldvt_testcase_20(HDMI_VIDEO_2160p_DSC_30Hz); */
			msleep(500);
			break;
		}
	case 21:
		{
/* dpi_dsc_testcase(HDMI_VIDEO_1080p_DSC_60Hz); */
/* dpi_dsc_testcase(HDMI_VIDEO_2160p_DSC_30Hz); */
/* dpi_dsc_testcase(HDMI_VIDEO_2160p_DSC_24Hz); */
			msleep(500);
			break;
		}
#endif
	default:
		DDPERR("case number is invailed, case: %d\n", caseNum);
	}

	return 0;
}

#if 0

static void BW_Worker_handler(struct work_struct *worker)
{
	int bandwidth = 0;

	/* bandwidth = bw_mon_in_ms(R, 10); */
	DDPERR("Display bandwidth for ldvt: bandwidth in last 10ms is %d\n",
	       bandwidth);
}

static void BW_Timer_handler(unsigned long timer)
{
	schedule_work(&bw_work);

	del_timer(&bw_timer);
	bw_timer.function = BW_Timer_handler;
	bw_timer.data = 0;
	bw_timer.expires = jiffies + 10;
	add_timer(&bw_timer);
}

int init_bw_monitor_timer(void)
{
	if (is_timer_inited)
		return 0;

	is_timer_inited = 1;
	INIT_WORK(&bw_work, BW_Worker_handler);

	init_timer(&bw_timer);
	bw_timer.function = BW_Timer_handler;
	bw_timer.data = 0;
	bw_timer.expires = jiffies + HZ;
	add_timer(&bw_timer);

	return 0;
}

int del_bw_monitor_timer(void)
{
	if (!is_timer_inited)
		return 0;

	del_timer(&bw_timer);
	is_timer_inited = 0;

	return 0;
}

unsigned int dpi_ldvt_testcase(void)
{
	int ret = 0;

	hdmi_factory_mode_test(STEP1_CHIP_INIT, NULL);
	init_bw_monitor_timer();
	dpi_dvt_ioctl(20);
	del_bw_monitor_timer();
	return ret;
}
#endif
#if 0
int dvt_dsi1_param(unsigned int mode, unsigned int resolution)
{
	unsigned int dpi_clock = 0;
	unsigned int hsync_pulse_width = 0, hsync_back_porch =
	    0, hsync_front_porch = 0, vsync_pulse_width = 0, vsync_back_porch =
	    0, vsync_front_porch = 0;
	unsigned int height = 0, width = 0;

	memset(&extd_dsi1_params, 0, sizeof(extd_dsi1_params));

	if (mode == DSI_VDO_MODE_BURST || mode == DSI_VDO_MODE_SYNC_EVENT
	    || mode == DSI_VDO_MODE_SYNC_PLUSE) {
		extd_dsi1_params.dispif_config.dsi.mode = mode;
		extd_dsi1_params.dispif_config.dsi.switch_mode = 0;
		extd_dsi1_params.dispif_config.dsi.switch_mode_enable = 0;
		extd_dsi1_params.dispif_config.dsi.LANE_NUM = 4;
		extd_dsi1_params.dispif_config.dsi.data_format.color_order =
		    LCM_COLOR_ORDER_RGB;
		extd_dsi1_params.dispif_config.dsi.data_format.trans_seq =
		    LCM_DSI_TRANS_SEQ_MSB_FIRST;
		extd_dsi1_params.dispif_config.dsi.data_format.padding =
		    LCM_DSI_PADDING_ON_LSB;
		extd_dsi1_params.dispif_config.dsi.data_format.format =
		    LCM_DSI_FORMAT_RGB888;

		extd_dsi1_params.dispif_config.dsi.packet_size = 256;
		extd_dsi1_params.dispif_config.dsi.PS = PACKED_PS_24BIT_RGB888;

	}

	switch (resolution) {
	case HDMI_VIDEO_720x480p_60Hz:
		{
			hsync_pulse_width = 62;
			hsync_back_porch = 60;
			hsync_front_porch = 16;

			vsync_pulse_width = 6;
			vsync_back_porch = 30;
			vsync_front_porch = 9;

			dpi_clock = 83;


			height = 480;
			width = 720;
			break;
		}
	case HDMI_VIDEO_720x480i_60Hz:
		{
			break;
		}
	case HDMI_VIDEO_1280x720p_60Hz:
		{
			hsync_pulse_width = 40;
			hsync_back_porch = 220;
			hsync_front_porch = 110;

			vsync_pulse_width = 5;
			vsync_back_porch = 20;
			vsync_front_porch = 5;
/* dpi_clock = 244; */
			dpi_clock = 228;


			height = 720;
			width = 1280;

			break;
		}
	case HDMI_VIDEO_1920x1080p_30Hz:
		{

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 88;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			dpi_clock = 228;
			/* dpi_clock = 228 << 8; */

			height = 1080;
			width = 1920;
			break;
		}
	case HDMI_VIDEO_1920x1080p_60Hz:
		{
			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 88;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			/* dpi_clock = 490; */
			dpi_clock = (459);

			height = 1080;
			width = 1920;
			break;
		}
	case HDMI_VIDEO_1080p_DSC_60Hz:
		{
			hsync_pulse_width = 15;
			hsync_back_porch = 49;
			hsync_front_porch = 29;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			dpi_clock = 165;
			/* dpi_clock = 153 << 8; */

			height = 1080;
			width = 640;
			break;
		}
	case HDMI_VIDEO_1920x1080i_60Hz:
		{
			break;
		}

	case HDMI_VIDEO_2160p_DSC_24Hz:
		{
			hsync_pulse_width = 30;
			hsync_back_porch = 98;
			hsync_front_porch = 242;

			vsync_pulse_width = 10;
			vsync_back_porch = 72;
			vsync_front_porch = 8;

			dpi_clock = 275;

			height = 2160;
			width = 1280;
			break;
		}
	case HDMI_VIDEO_2160p_DSC_30Hz:
		{
			hsync_pulse_width = 30;
			hsync_back_porch = 98;
			hsync_front_porch = 66;

			vsync_pulse_width = 10;
			vsync_back_porch = 72;
			vsync_front_porch = 8;

/* dpi_clock = 308 << 8; */
			dpi_clock = 308;

			height = 2160;
			width = 1280;
			break;
		}

	default:
		break;
	}

	extd_dsi1_params.dispif_config.dsi.horizontal_sync_active =
	    hsync_pulse_width;
	extd_dsi1_params.dispif_config.dsi.horizontal_backporch =
	    hsync_back_porch;
	extd_dsi1_params.dispif_config.dsi.horizontal_frontporch =
	    hsync_front_porch;
	extd_dsi1_params.dispif_config.dsi.horizontal_active_pixel = width;

	extd_dsi1_params.dispif_config.dsi.vertical_sync_active =
	    vsync_pulse_width;
	extd_dsi1_params.dispif_config.dsi.vertical_backporch =
	    vsync_back_porch;
	extd_dsi1_params.dispif_config.dsi.vertical_frontporch =
	    vsync_front_porch;
	extd_dsi1_params.dispif_config.dsi.vertical_active_line = height;

	extd_dsi1_params.dispif_config.dsi.PLL_CLOCK = dpi_clock;
	extd_dsi1_params.dispif_config.dsi.PLL_CK_VDO = dpi_clock;
	extd_dsi1_params.dispif_config.dsi.cont_clock = TRUE;
	extd_dsi1_params.dispif_config.dsi.dsc_enable = 1;
	extd_dsi1_params.dispif_config.dsi.word_count = width * 3;

	extd_dsi1_params.dst_dirty = 1;
	extd_dsi1_params.dst_h = height;
	extd_dsi1_params.dst_w = width;


	return 0;
}

static int dpi_dsc_testcase(unsigned int resolution)
{
	int ret = 0;

	DPI_DVT_LOG_W("dpi_ldvt_testcase_20, resolution: %d\n", resolution);
	dpi_dvt_parameters(resolution);

	DPI_DVT_LOG_W("top clock on\n");
	dvt_ddp_path_top_clock_on();
	/* set rdma shadow register bypass mode */
	DISP_REG_SET(NULL,
		     1 * (DDP_REG_BASE_DISP_RDMA1 - DDP_REG_BASE_DISP_RDMA0)
		     + DDP_REG_BASE_DISP_RDMA0 + DISP_REG_RDMA_SHADOW_UPDATE,
		     (0x1 << 1) | (0x1 << 2));
	DISP_REG_SET(NULL, DDP_REG_BASE_DISP_DSC + DISP_REG_DSC_SHADOW,
		     (0x1 << 1) | (0x1 << 2));

	DPI_DVT_LOG_W("set Mutex and connect path\n");
	dvt_connect_path(NULL);
	dvt_mutex_set(dvt_acquire_mutex(), NULL);

	ddp_driver_dpi.init(DISP_MODULE_DPI, NULL);
	ddp_driver_dsc.init(DISP_MODULE_DSC, NULL);
	ddp_driver_rdma.init(DISP_MODULE_RDMA1, NULL);
	ddp_driver_dpi.power_on(DISP_MODULE_DPI, NULL);
	ddp_driver_dsc.power_on(DISP_MODULE_DSC, NULL);
	ddp_driver_rdma.power_on(DISP_MODULE_RDMA1, NULL);

	ret = dvt_allocate_buffer(resolution, M4U_FOR_RDMA1);
	dvt_init_RDMA_param(RDMA_MODE_MEMORY, resolution);
	dvt_init_DSC_param(resolution);
/* dvt_dsi1_param(DSI_VDO_MODE_SYNC_PLUSE, resolution); */

	ddp_driver_dsc.config(DISP_MODULE_DSC, &extd_dpi_params, NULL);
	ddp_driver_dpi.config(DISP_MODULE_DPI, &extd_dpi_params, NULL);
	ddp_driver_rdma.config(DISP_MODULE_RDMA1, &extd_rdma_params, NULL);
	ddp_driver_dpi.reset(DISP_MODULE_DPI, NULL);
	ddp_driver_dsc.reset(DISP_MODULE_DSC, NULL);
	ddp_driver_rdma.reset(DISP_MODULE_RDMA1, NULL);
	ddp_driver_dpi.start(DISP_MODULE_DPI, NULL);
	ddp_driver_dsc.start(DISP_MODULE_DSC, NULL);
	ddp_driver_rdma.start(DISP_MODULE_RDMA1, NULL);

	dvt_mutex_enable(NULL, dvt_acquire_mutex());

	ddp_driver_dpi.trigger(DISP_MODULE_DPI, NULL);

	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 1);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 0);

	return 0;
}
#endif
#if 0
static int dsi_dsc_testcase(unsigned int resolution)
{
	int ret = 0;

	DPI_DVT_LOG_W("dpi_ldvt_testcase_20, resolution: %d\n", resolution);
	dpi_dvt_parameters(resolution);

	DPI_DVT_LOG_W("top clock on\n");
	/* dvt_ddp_path_top_clock_on(); */
	dvt_ddp_path_top_clock_on();
	/* set rdma shadow register bypass mode */
	DISP_REG_SET(NULL,
		     1 * (DDP_REG_BASE_DISP_RDMA1 - DDP_REG_BASE_DISP_RDMA0)
		     + DDP_REG_BASE_DISP_RDMA0 + DISP_REG_RDMA_SHADOW_UPDATE,
		     (0x1 << 1) | (0x1 << 2));
	DISP_REG_SET(NULL, DDP_REG_BASE_DISP_DSC + DISP_REG_DSC_SHADOW,
		     (0x1 << 1) | (0x1 << 2));

	DPI_DVT_LOG_W("set Mutex and connect path\n");
	dsi_dsc_dvt_connect_path(NULL);
	dsi_dsc_dvt_mutex_set(dvt_acquire_mutex(), NULL);

	ddp_driver_dsi1.init(DISP_MODULE_DSI1, NULL);
	ddp_driver_dsc.init(DISP_MODULE_DSC, NULL);
	ddp_driver_rdma.init(DISP_MODULE_RDMA1, NULL);
	ddp_driver_dsi1.power_on(DISP_MODULE_DSI1, NULL);
	ddp_driver_dsc.power_on(DISP_MODULE_DSC, NULL);
	ddp_driver_rdma.power_on(DISP_MODULE_RDMA1, NULL);

	ret = dvt_allocate_buffer(resolution, M4U_FOR_RDMA1);
	dvt_init_RDMA_param(RDMA_MODE_MEMORY, resolution);
	dvt_init_DSC_param(resolution);
	dvt_dsi1_param(DSI_VDO_MODE_SYNC_PLUSE, resolution);

	ddp_driver_dsc.config(DISP_MODULE_DSC, &extd_dpi_params, NULL);
	ddp_driver_dsi1.config(DISP_MODULE_DSI1, &extd_dsi1_params, NULL);
	ddp_driver_rdma.config(DISP_MODULE_RDMA1, &extd_rdma_params, NULL);
	ddp_driver_dsi1.reset(DISP_MODULE_DSI1, NULL);
	ddp_driver_dsc.reset(DISP_MODULE_DSC, NULL);
	ddp_driver_rdma.reset(DISP_MODULE_RDMA1, NULL);
	ddp_driver_dsi1.start(DISP_MODULE_DSI1, NULL);
	ddp_driver_dsc.start(DISP_MODULE_DSC, NULL);
	ddp_driver_rdma.start(DISP_MODULE_RDMA1, NULL);

	dvt_mutex_enable(NULL, dvt_acquire_mutex());
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 1);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 0);

	ddp_driver_dsi1.trigger(DISP_MODULE_DSI1, NULL);

	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 1);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 0);

	msleep(20);
	ddp_driver_dsi1.dump_info(DISP_MODULE_DSI1, 1);
	ddp_driver_dsc.dump_info(DISP_MODULE_DSC, 1);

	return 0;
}
#endif
#if 0
static int ovl_dsi_dsc_testcase(unsigned int resolution)
{
	int ret = 0;
	struct ddp_io_golden_setting_arg gset_arg;

	DPI_DVT_LOG_W("dpi_ldvt_testcase_20, resolution: %d\n", resolution);
	dpi_dvt_parameters(resolution);

	DPI_DVT_LOG_W("top clock on\n");
	dvt_ddp_path_top_clock_on();
	/* set rdma shadow register bypass mode */
	DISP_REG_SET(NULL,
		     1 * (DDP_REG_BASE_DISP_RDMA1 - DDP_REG_BASE_DISP_RDMA0)
		     + DDP_REG_BASE_DISP_RDMA0 + DISP_REG_RDMA_SHADOW_UPDATE,
		     (0x1 << 1) | (0x1 << 2));
	DISP_REG_SET(NULL, DDP_REG_BASE_DISP_DSC + DISP_REG_DSC_SHADOW,
		     (0x1 << 1) | (0x1 << 2));
	DISP_REG_SET(NULL,
		     DDP_REG_BASE_DISP_GAMMA1 - DDP_REG_BASE_DISP_GAMMA0 +
		     DISP_REG_GAMMA_DEBUG, (0x1 << 1) | (0x1 << 2));
	DISP_REG_SET(NULL,
		     DDP_REG_BASE_DISP_COLOR1 - DDP_REG_BASE_DISP_COLOR0 +
		     DISP_COLOR_SHADOW_CTRL, (0x1 << 0) | (0x1 << 2));
	DISP_REG_SET(NULL,
		     DDP_REG_BASE_DISP_CCORR1 - DDP_REG_BASE_DISP_CCORR0 +
		     DISP_REG_CCORR_SHADOW, (0x1 << 0) | (0x1 << 2));
	DISP_REG_SET(NULL,
		     DDP_REG_BASE_DISP_AAL1 - DDP_REG_BASE_DISP_AAL0 +
		     DISP_AAL_SHADOW_CTL, (0x1 << 0) | (0x1 << 2));
	DISP_REG_SET(NULL,
		     DDP_REG_BASE_DISP_DITHER1 - DDP_REG_BASE_DISP_DITHER0 +
		     DISP_REG_DITHER_0, (0x1 << 0) | (0x1 << 2));

	DISP_REG_SET_FIELD(NULL, EN_FLD_BYPASS_SHADOW, DDP_REG_BASE_DISP_OVL1
			   + DISP_REG_OVL_EN, 0x1);
	DISP_REG_SET_FIELD(NULL, EN_FLD_RD_WRK_REG, DDP_REG_BASE_DISP_OVL1
			   + DISP_REG_OVL_EN, 0x1);
	DISP_REG_SET_FIELD(NULL, EN_FLD_BYPASS_SHADOW, DDP_REG_BASE_DISP_OVL1_2L
			   + DISP_REG_OVL_EN, 0x1);
	DISP_REG_SET_FIELD(NULL, EN_FLD_RD_WRK_REG, DDP_REG_BASE_DISP_OVL1_2L
			   + DISP_REG_OVL_EN, 0x1);

	DPI_DVT_LOG_W("set Mutex and connect path\n");
	ovl_dsi_dsc_dvt_connect_path(NULL);
	ovl_dsi_dsc_dvt_mutex_set(dvt_acquire_mutex(), NULL);

	ddp_driver_ovl.connect(DISP_MODULE_OVL1, DISP_MODULE_UNKNOWN,
			       DISP_MODULE_OVL1_2L, 1, NULL);
	ddp_driver_ovl.connect(DISP_MODULE_OVL1_2L, DISP_MODULE_OVL1,
			       DISP_MODULE_UNKNOWN, 1, NULL);

	DPI_DVT_LOG_W("module init\n");
	dvt_module_action(LONG_PATH1_DSI1, INIT);
	DPI_DVT_LOG_W("module init done\n");

	DPI_DVT_LOG_W("module power on\n");
	dvt_module_action(LONG_PATH1_DSI1, POWER_ON);

	ret = dvt_allocate_buffer(resolution, M4U_FOR_OVL1);
	dvt_init_RDMA_param(RDMA_MODE_DIRECT_LINK, resolution);
	dvt_init_DSC_param(resolution);
	dvt_dsi1_param(DSI_VDO_MODE_SYNC_PLUSE, resolution);
	dvt_init_GAMMA_param(RDMA_MODE_DIRECT_LINK);
	ddp_driver_gamma.config(DISP_MODULE_GAMMA1, &extd_gamma_params, NULL);
	ddp_driver_aal.config(DISP_MODULE_AAL1, &extd_gamma_params, NULL);
	ddp_driver_color.config(DISP_MODULE_COLOR1, &extd_gamma_params, NULL);
	ddp_driver_ccorr.config(DISP_MODULE_CCORR1, &extd_gamma_params, NULL);
	ddp_driver_dither.config(DISP_MODULE_DITHER1, &extd_gamma_params, NULL);
	DISP_REG_SET(NULL,
		     DISPSYS_COLOR1_BASE - DISPSYS_COLOR0_BASE +
		     DISP_COLOR_CK_ON, 0x1);

	ddp_driver_dsc.config(DISP_MODULE_DSC, &extd_dpi_params, NULL);
	ddp_driver_dsi1.config(DISP_MODULE_DSI1, &extd_dsi1_params, NULL);
	ddp_driver_rdma.config(DISP_MODULE_RDMA1, &extd_rdma_params, NULL);

	dsi_init_OVL_param(RDMA_MODE_DIRECT_LINK);
	ddp_driver_ovl.config(DISP_MODULE_OVL1, &extd_ovl_params, NULL);
	ddp_driver_ovl.config(DISP_MODULE_OVL1_2L, &extd_ovl_params, NULL);

	memset(&gset_arg, 0, sizeof(gset_arg));
	gset_arg.dst_mod_type = DST_MOD_REAL_TIME;
	gset_arg.is_decouple_mode = 0;
	ddp_driver_ovl.ioctl(DISP_MODULE_OVL1, NULL, DDP_OVL_GOLDEN_SETTING,
			     &gset_arg);
	ddp_driver_ovl.ioctl(DISP_MODULE_OVL1_2L, NULL, DDP_OVL_GOLDEN_SETTING,
			     &gset_arg);

	DPI_DVT_LOG_W("module reset\n");
	dvt_mutex_reset(NULL, dvt_acquire_mutex());
	dvt_module_action(LONG_PATH1_DSI1, RESET);

	DPI_DVT_LOG_W("module start\n");
	dvt_module_action(LONG_PATH1_DSI1, START);
	dvt_mutex_enable(NULL, dvt_acquire_mutex());

	ddp_driver_dsi1.trigger(DISP_MODULE_DSI1, NULL);

	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 1);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 0);

	return 0;
}
#endif
/* 1280 */
#if 0
unsigned int dpi_ldvt_testcase(void)
{
	int ret = 0;
/* #if 0 */
	int i = 0;
	unsigned int reg = 0;
	unsigned char *para;

	para = kmalloc(8, GFP_KERNEL);
	if (!para)
		return 0;
	for (i = 0; i < 8; i++)
		para[i] = 0x5a;

/* pmic_set_register_value(PMIC_RG_VGP3_VOSEL, 0x0); */
/* pmic_set_register_value(PMIC_RG_VGP3_SW_EN, 1); */

	dpi_dvt_parameters(HDMI_VIDEO_2160p_DSC_30Hz);
/* dpi_dvt_parameters(HDMI_VIDEO_1280x720p_60Hz); */


	DPI_DVT_LOG_W("top clock on\n");
	dvt_ddp_path_top_clock_on();

	DPI_DVT_LOG_W("set Mutex and connect path\n");
	dsi_dvt_connect_path(NULL);

	dsi_dvt_mutex_set(dvt_acquire_mutex(), NULL);


	ddp_driver_dsi1.init(DISP_MODULE_DSI1, NULL);
	ddp_driver_rdma.init(DISP_MODULE_RDMA1, NULL);
	ddp_driver_dsi1.power_on(DISP_MODULE_DSI1, NULL);
	ddp_driver_rdma.power_on(DISP_MODULE_RDMA1, NULL);

	ret = dvt_allocate_buffer(HDMI_VIDEO_2160p_DSC_30Hz, M4U_FOR_RDMA1);
	dvt_init_RDMA_param(RDMA_MODE_MEMORY, HDMI_VIDEO_2160p_DSC_30Hz);
/* ret = dvt_allocate_buffer(HDMI_VIDEO_1280x720p_60Hz, M4U_FOR_RDMA1); */
/* dvt_init_RDMA_param(RDMA_MODE_MEMORY, HDMI_VIDEO_1280x720p_60Hz); */


/* dvt_dsi1_param(DSI_VDO_MODE_SYNC_PLUSE, HDMI_VIDEO_1280x720p_60Hz); */
	dvt_dsi1_param(DSI_VDO_MODE_SYNC_PLUSE, HDMI_VIDEO_2160p_DSC_30Hz);

	ddp_driver_dsi1.config(DISP_MODULE_DSI1, &extd_dsi1_params, NULL);
	ddp_driver_rdma.config(DISP_MODULE_RDMA1, &extd_rdma_params, NULL);
	ddp_driver_dsi1.reset(DISP_MODULE_DSI1, NULL);
	ddp_driver_rdma.reset(DISP_MODULE_RDMA1, NULL);
	ddp_driver_dsi1.start(DISP_MODULE_DSI1, NULL);
	ddp_driver_rdma.start(DISP_MODULE_RDMA1, NULL);

	dvt_mutex_enable(NULL, dvt_acquire_mutex());

/* DSI_BIST_Pattern_Test(DISP_MODULE_DSI1, NULL, true, 0x0000ff00); */

	ddp_driver_dsi1.trigger(DISP_MODULE_DSI1, NULL);

	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 1);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 0);

	reg = DISP_REG_GET(&DSI_REG[1]->DSI_TXRX_CTRL);
	DPI_DVT_LOG_W("DSI_TXRX_CON reg = 0x%x\n", reg);



	DSI_set_cmdq_V2(DISP_MODULE_DSI1, NULL, 0xb1, 7, para, 1);
/* DSI_BIST_Pattern_Test(DISP_MODULE_DSI1, NULL, true, 0x0000ff00); */
	kfree(para);
/* #endif */
/* dpi_dvt_ioctl(10); */
/* dsi_dsc_testcase(HDMI_VIDEO_2160p_DSC_30Hz); */
/* dsi_dsc_testcase(HDMI_VIDEO_1080p_DSC_60Hz); */
/* ovl_dsi_dsc_testcase(HDMI_VIDEO_1080p_DSC_60Hz); */
	/* pmic_set_register_value(PMIC_RG_VGP3_VOSEL, 0x0); */
	/* pmic_set_register_value(PMIC_RG_VGP3_SW_EN, 1); */

	hdmi_factory_mode_test(STEP1_CHIP_INIT, NULL);

/* dpi_dvt_testcase_4_timing(HDMI_VIDEO_1920x1080p_60Hz); */
/* dpi_dvt_ioctl(20); */
/* dpi_dvt_ioctl(21); */
/* dpi_dvt_testcase_4_timing(HDMI_VIDEO_1920x1080p_30Hz); */
/* dpi_dvt_testcase_4_timing(HDMI_VIDEO_1280x720p_60Hz); */
/* dpi_dvt_testcase_4_timing(HDMI_VIDEO_720x480p_60Hz); */

	return ret;

}
#endif
#if 0
unsigned int dpi_ldvt_testcase(void)
{
	int ret = 0;
#if 0
	int i = 0;
/* unsigned int reg = 0; */
	unsigned char *para;

	para = kmalloc(8, GFP_KERNEL);
	if (!para)
		return 0;
	for (i = 0; i < 8; i++)
		para[i] = 0x5a;

/* pmic_set_register_value(PMIC_RG_VGP3_VOSEL, 0x0); */
/* pmic_set_register_value(PMIC_RG_VGP3_SW_EN, 1); */

	dpi_dvt_parameters(HDMI_VIDEO_1920x1080p_30Hz);
/* dpi_dvt_parameters(HDMI_VIDEO_1280x720p_60Hz); */


	DPI_DVT_LOG_W("top clock on\n");
	dvt_ddp_path_top_clock_on();

	DPI_DVT_LOG_W("set Mutex and connect path\n");
	dsi_dvt_connect_path(NULL);

	dsi_dvt_mutex_set(dvt_acquire_mutex(), NULL);


	ddp_driver_dsi1.init(DISP_MODULE_DSI1, NULL);
	ddp_driver_rdma.init(DISP_MODULE_RDMA1, NULL);
	ddp_driver_dsi1.power_on(DISP_MODULE_DSI1, NULL);
	ddp_driver_rdma.power_on(DISP_MODULE_RDMA1, NULL);

	ret = dvt_allocate_buffer(HDMI_VIDEO_1920x1080p_30Hz, M4U_FOR_RDMA1);
	dvt_init_RDMA_param(RDMA_MODE_MEMORY, HDMI_VIDEO_1920x1080p_30Hz);
/* ret = dvt_allocate_buffer(HDMI_VIDEO_1280x720p_60Hz, M4U_FOR_RDMA1); */
/* dvt_init_RDMA_param(RDMA_MODE_MEMORY, HDMI_VIDEO_1280x720p_60Hz); */


/* dvt_dsi1_param(DSI_VDO_MODE_SYNC_PLUSE, HDMI_VIDEO_1280x720p_60Hz); */
	dvt_dsi1_param(DSI_VDO_MODE_SYNC_PLUSE, HDMI_VIDEO_1920x1080p_30Hz);

	ddp_driver_dsi1.config(DISP_MODULE_DSI1, &extd_dsi1_params, NULL);
	ddp_driver_rdma.config(DISP_MODULE_RDMA1, &extd_rdma_params, NULL);
	ddp_driver_dsi1.reset(DISP_MODULE_DSI1, NULL);
	ddp_driver_rdma.reset(DISP_MODULE_RDMA1, NULL);
	ddp_driver_dsi1.start(DISP_MODULE_DSI1, NULL);
	ddp_driver_rdma.start(DISP_MODULE_RDMA1, NULL);

	dvt_mutex_enable(NULL, dvt_acquire_mutex());

/* DSI_BIST_Pattern_Test(DISP_MODULE_DSI1, NULL, true, 0x0000ff00); */

	ddp_driver_dsi1.trigger(DISP_MODULE_DSI1, NULL);

	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 1);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_GET(1), 0);

/* reg = DISP_REG_GET(&DSI_REG[1]->DSI_TXRX_CTRL); */
/* DPI_DVT_LOG_W("DSI_TXRX_CON reg = 0x%x\n", reg); */



/* DSI_set_cmdq_V2(DISP_MODULE_DSI1, NULL, 0xb1, 7, para,1); */
/* DSI_BIST_Pattern_Test(DISP_MODULE_DSI1, NULL, true, 0x0000ff00); */
	kfree(para);
#endif
/* dpi_dvt_ioctl(10); */
	dsi_dsc_testcase(HDMI_VIDEO_2160p_DSC_30Hz);
/* dsi_dsc_testcase(HDMI_VIDEO_2160p_DSC_24Hz); */
/* dsi_dsc_testcase(HDMI_VIDEO_1080p_DSC_60Hz); */
/* ovl_dsi_dsc_testcase(HDMI_VIDEO_1080p_DSC_60Hz); */
	/* pmic_set_register_value(PMIC_RG_VGP3_VOSEL, 0x0); */
	/* pmic_set_register_value(PMIC_RG_VGP3_SW_EN, 1); */

	hdmi_factory_mode_test(STEP1_CHIP_INIT, NULL);

/* dpi_dvt_testcase_4_timing(HDMI_VIDEO_1920x1080p_60Hz); */
/* dpi_dvt_ioctl(20); */
/* dpi_dvt_ioctl(21); */
/* dpi_dvt_testcase_4_timing(HDMI_VIDEO_1920x1080p_30Hz); */
/* dpi_dvt_testcase_4_timing(HDMI_VIDEO_1280x720p_60Hz); */
/* dpi_dvt_testcase_4_timing(HDMI_VIDEO_720x480p_60Hz); */

	return ret;

}
#endif

unsigned int dpi_dvt_ioctl(unsigned int arg)
{
	int ret = 0;

	/* get HDMI Driver from vendor floder */
	hdmi_drv = (struct HDMI_DRIVER *)HDMI_GetDriver();
	if (hdmi_drv == NULL) {
		DPI_DVT_LOG_W
		    ("[hdmi]%s, init fail, can't get hdmi driver handle\n",
		     __func__);
		return -1;
	}

	DPI_DVT_LOG_W("testcase: %x\n", arg);
	dpi_dvt_run_cases(arg);

	return ret;
}

#else
unsigned int dpi_dvt_ioctl(unsigned int arg)
{
	return 0;
}
#endif
