// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

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


#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/types.h>
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#endif

#if defined(CONFIG_MTK_M4U)
#include "m4u.h"
#endif
#include "ddp_log.h"
#include "cmdq_record.h"
#include "dpi_dvt_platform.h"

#include "ddp_clkmgr.h"
#include "ddp_reg.h"
#include "dpi_dvt_test.h"
#include "disp_helper.h"

#undef kal_uint32
#define kal_uint32 unsigned int

/*static bool top_power_on;*/
#define SOF_DPI0 0xC3
#define SOF_DSI1 0x82

unsigned int long_path1_module[LONG_PATH1_MODULE_NUM] = {
	DISP_MODULE_OVL1_2L,
	DISP_MODULE_RDMA1,
	DISP_MODULE_DPI
};

unsigned int short_path_module[SHORT_PATH_MODULE_NUM] = {
	DISP_MODULE_RDMA1,
	DISP_MODULE_DPI
};

int init_cmdq_slots(cmdqBackupSlotHandle *pSlot, int count, int init_val)
{
	int i;

	cmdqBackupAllocateSlot(pSlot, count);
	for (i = 0; i < count; i++)
		cmdqBackupWriteSlot(*pSlot, i, init_val);
	return 0;
}

int dvt_connect_path(void *handle)
{
	kal_uint32 value = 0;

	/*RDMA1_SOUT to DPI0_SEL */
	value = 0;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_RDMA1_SOUT_SEL_IN, value);

	/*DPI0_SEL from RDMA1_SOUT */
	value = 2;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_DPI0_SEL_IN, value);

	return 0;
}

int dvt_disconnect_path(void *handle)
{
	kal_uint32 value = 0;

	/*RDMA1_SOUT to DPI0_SEL */
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_RDMA1_SOUT_SEL_IN, value);

	/*DPI0_SEL from RDMA1_SOUT */
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_DPI0_SEL_IN, value);

	return 0;
}

int dvt_acquire_mutex(void)
{
	return HW_MUTEX;
}

int dvt_ddp_path_top_clock_on(void)
{
	ddp_path_top_clock_on();

	return 0;
}

int dvt_ddp_path_top_clock_off(void)
{
	ddp_path_top_clock_off();

	return 0;
}

static char *dvt_ddp_get_mutex_sof_name(unsigned int regval)
{
	if (regval == SOF_VAL_MUTEX0_SOF_SINGLE_MODE)
		return "single";
	else if (regval == SOF_VAL_MUTEX0_SOF_FROM_DSI0)
		return "dsi0";
	else if (regval == SOF_VAL_MUTEX0_SOF_FROM_DPI)
		return "dpi";

	DDPDUMP("%s, unknown reg=%d\n", __func__, regval);
	return "unknown";
}

static int dvt_mutex_enable_l(void *handle, unsigned int hwmutex_id)
{
	DPI_DVT_LOG_W("mutex %d enable\n", hwmutex_id);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_EN(hwmutex_id), 1);
	return 0;
}

int dvt_mutex_enable(void *handle, unsigned int hwmutex_id)
{
	return dvt_mutex_enable_l(handle, hwmutex_id);
}

int dvt_mutex_disenable(void *handle, unsigned int hwmutex_id)
{
	DPI_DVT_LOG_W("mutex %d disable\n", hwmutex_id);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_EN(hwmutex_id), 0);
	return 0;
}

int dvt_mutex_reset(void *handle, unsigned int hwmutex_id)
{
	DPI_DVT_LOG_W("mutex %d reset\n", hwmutex_id);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_RST(hwmutex_id), 1);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_RST(hwmutex_id), 0);

	return 0;
}

static int dvt_mutex_mod(void *handle, unsigned int hwmutex_id, int mode,
			 unsigned int num)
{
	if (num == 0)
		DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_MOD0(hwmutex_id),
			     mode);
	return 0;
}

static int dvt_mutex_sof(void *handle, unsigned int hwmutex_id, int sof)
{
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_SOF(hwmutex_id), sof);
	return 0;
}

/*{DISP_MODULE_RDMA_SHORT    , 15}*/
int dvt_mutex_set(unsigned int mutex_id, void *handle)
{
	kal_uint32 mode = 0;
	/* int sof = SOF_DPI0; */

	int sof = 0x82;

	mode |= 1 << RDMA_MOD_FOR_MUTEX_SHORT;

	if (mutex_id < DISP_MUTEX_DDP_FIRST || mutex_id > DISP_MUTEX_DDP_LAST) {
		DDPERR("exceed mutex max (0 ~ %d)\n", DISP_MUTEX_DDP_LAST);
		return -1;
	}

	dvt_mutex_mod(handle, mutex_id, mode, 0);
	dvt_mutex_sof(handle, mutex_id, sof);
	DPI_DVT_LOG_W("%s %d mode=0x%x, sof=%s, sof=%x\n", __func__, mutex_id,
		      mode, dvt_ddp_get_mutex_sof_name(sof), sof);

	return 0;
}

#ifdef DPI_DVT_TEST_SUPPORT

/*OVL1 -> OVL1_MOUT -> OVL1_2L_SEL -> OVL1_2L -> OVL1_PQ_MOUT ->*/
/*	COLOR1_SEL -> DITHER1_MOUT*/
/*	-> DISP_RDMA1 -> RDMA1_SOUT -> DPI_SEL -> DPI*/
/*ovl1 out, rdma1 sel, dpi sel*/
int dvt_connect_ovl1_dpi(void *handle)
{
	kal_uint32 value = 0;

	/*OVL1_2L_MOUT to RDMA1 */
	value = 0x10;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_OVL1_2L_MOUT_EN, value);

	/*RDMA1_SOUT to DPI0_SEL */
	value = 0;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_RDMA1_SOUT_SEL_IN, value);

	/*DPI0_SEL from RDMA1_SOUT */
	value = 2;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_DPI0_SEL_IN, value);


	return 0;
}

int dvt_disconnect_ovl1_dpi(void *handle)
{
	kal_uint32 value = 0;

	/*OVL1_2L_MOUT to RDMA1 */
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_OVL1_2L_MOUT_EN, value);

	/*RDMA1_SOUT to DPI0_SEL */
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_RDMA1_SOUT_SEL_IN, value);

	/*DPI0_SEL from RDMA1_SOUT */
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_DPI0_SEL_IN, value);


	return 0;
}

/*OVL1 -> OVL1_MOUT -> DISP_RDMA1 -> RDMA1_SOUT -> DPI_SEL -> DPI*/
int dvt_mutex_set_ovl1_dpi(unsigned int mutex_id, void *handle)
{
	kal_uint32 mode = 0;
	int sof = 0x82;

	mode |= 1 << OVL1_2L_MOD_FOR_MUTEX;
	mode |= 1 << RDMA_MOD_FOR_MUTEX_SHORT;

	if (mutex_id < DISP_MUTEX_DDP_FIRST || mutex_id > DISP_MUTEX_DDP_LAST) {
		DDPERR("exceed mutex max (0 ~ %d)\n", DISP_MUTEX_DDP_LAST);
		return -1;
	}

	dvt_mutex_mod(handle, mutex_id, mode, 0);
	dvt_mutex_sof(handle, mutex_id, sof);
	DPI_DVT_LOG_W("dvt_mutex_set %d mode=0x%x, sof=%s\n", mutex_id, mode,
		      dvt_ddp_get_mutex_sof_name(sof));

	return 0;
}
#endif
#endif
