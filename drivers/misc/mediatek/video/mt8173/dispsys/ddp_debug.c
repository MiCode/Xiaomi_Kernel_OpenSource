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

#define LOG_TAG "DEBUG"

#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <mt-plat/aee.h>
#include "disp_assert_layer.h"
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/time.h>

#include "m4u.h"
#include "m4u_port.h"

#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"

#include "disp_drv_ddp.h"

#include "ddp_debug.h"
#include "ddp_reg.h"
#include "ddp_drv.h"
#include "ddp_wdma.h"
#include "ddp_hal.h"
#include "ddp_path.h"
#include "ddp_aal.h"
#include "ddp_pwm.h"
#include "ddp_info.h"
#include "ddp_dsi.h"
#include "ddp_ovl.h"
#include "ddp_irq.h"

#include "ddp_manager.h"
#include "ddp_log.h"
#include "ddp_met.h"
#include "display_recorder.h"
#include "disp_session.h"
#include "primary_display.h"
#include "disp_drv_platform.h"

#ifdef CONFIG_MTK_HDMI_SUPPORT
#include "extd_ddp.h"
#endif
#pragma GCC optimize("O0")


/* --------------------------------------------------------------------------- */
/* External variable declarations */
/* --------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------- */
/* Debug Options */
/* --------------------------------------------------------------------------- */

static struct dentry *debugfs;
static struct dentry *debugDir;
static struct dentry *debugfs_dump;

static const long int DEFAULT_LOG_FPS_WND_SIZE = 30;
static int debug_init;
unsigned char pq_debug_flag = 0;
unsigned char aal_debug_flag = 0;
static unsigned int dbg_log_level;
static unsigned int irq_log_level;
static unsigned int dump_to_buffer;

unsigned int gOVLBackground = 0xFF000000;
unsigned int gUltraEnable = 1;
unsigned int gDumpMemoutCmdq = 0;
unsigned int gEnableUnderflowAEE = 0;

#ifdef CONFIG_MTK_TC8_TABLET_RELEASE_ONLY
unsigned int disp_low_power_reduse_fps = 1;
#else
unsigned int disp_low_power_reduse_fps = 0;
#endif
unsigned int disp_low_power_reduse_clock = 0;
unsigned int disp_low_power_adjust_vfp = 0;
unsigned int disp_low_power_disable_ddp_clock = 1;
unsigned int disp_low_power_remove_ovl = 1;
unsigned int gSkipIdleDetect = 0;
unsigned int gDumpClockStatus = 1;

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
unsigned int gDebugSvp = 0;
unsigned int gDebugSvpOption = 0;
#endif

#ifdef DISP_ENABLE_SODI
unsigned int gEnableSODIControl = 1;
unsigned int gPrefetchControl = 1;
#else
unsigned int gEnableSODIControl = 0;
unsigned int gPrefetchControl = 0;
#endif

/* enable it when use UART to grab log */
unsigned int gEnableUartLog = 0;
/* mutex SOF at raing edge of vsync, can save more time for cmdq config */
unsigned int gEnableMutexRisingEdge = 0;
/* only write dirty register, reduce register number write by cmdq */
unsigned int gEnableReduceRegWrite = 0;

unsigned int gDumpConfigCMD = 0;
unsigned int gDumpESDCMD = 0;

unsigned int gESDEnableSODI = 1;
unsigned int gEnableOVLStatusCheck = 1;

unsigned int gResetRDMAEnable = 1;
unsigned int gEnableSWTrigger = 0;

unsigned int gResetOVLInAALTrigger = 0;
unsigned int gDisableOVLTF = 0;

unsigned int gRDMAUltraSetting = 0x1b013ba0;	/* so we can modify RDMA ultra at run-time */
unsigned int gRDMAFIFOLen = 32;

#ifdef _MTK_USER_
unsigned int gEnableIRQ = 0;
/* #error eng_error */
#else
unsigned int gEnableIRQ = 1;
/* #error user_error */
#endif
unsigned int gDisableSODIForTriggerLoop = 1;

/*for MTK_HDMI_MAIN_PATH*/
unsigned int ddp_dbg_level = (0
/* | DDP_FUNC_LOG */
/* | DDP_FLOW_LOG */
/* | DDP_COLOR_FORMAT_LOG */
/* | DDP_FB_FLOW_LOG */
/* | DDP_RESOLUTION_LOG */
/* | DDP_OVL_FB_LOG */
/* | DDP_FENCE_LOG */
/* | DDP_TVE_FENCE_LOG */
/* | DDP_FENCE1_LOG */
/* | DDP_FENCE2_LOG */
);

void set_ddp_log_level(unsigned int level)
{
	DDPMSG("set ddp_dbg_level level %d\n", level);
	if (ddp_dbg_level & (1 << level)) {
		ddp_dbg_level &= ~(1 << level);
		DDPMSG("set ddp_dbg_level  0x%x\n", ddp_dbg_level);
	} else {
		ddp_dbg_level |= (1 << level);
		DDPMSG("set ddp_dbg_level  0x%x\n", ddp_dbg_level);
	}
}

static char STR_HELP[] =
"USAGE:\n"
"       echo [ACTION]>/d/dispsys\n"
"ACTION:\n"
"       regr:addr                :regr:va\n"
"       regw:addr,value          :regw:va 0x1\n"
"       regr_pa:addr             :regr_pa:0x1400c000\n"
"       regw_pa:addr,value       :regw_pa:0x1400c000,0x1\n"
"       g_regr:addr              :g_regr:0x1400c000\n"
"       g_regw:addr,value        :g_regw:0x1400c000,0x1\n"
"       dbg_log:0|1|2            :0 off, 1 dbg, 2 all\n"
"       irq_log:0|1              :0 off, !0 on\n"
"       met_on:[0|1] [0|1] [0|1] :fist[0|1]on|off,other [0|1]direct|decouple\n"
"       backlight:level\n"
"       dump_aal:arg\n"
"       mmp\n"
"       dump_module:module name     :dump_module:rdma0\n"
"       dump_path:mutexID\n" "       dpfd_ut1:channel\n"
"       lowpower:session,value   :\n"
"           session:0[fps]1[pllclk]2[vfp]3[ddpclk]4[removeovl]5[autoswitch]";
/* --------------------------------------------------------------------------- */
/* Command Processor */
/* --------------------------------------------------------------------------- */
static char dbg_buf[2048];
static unsigned int is_reg_addr_valid(unsigned int isVa, unsigned long addr)
{
	unsigned int i = 0;

	for (i = 0; i < DISP_MODULE_NUM; i++) {
		if ((isVa == 1) && (addr >= ddp_regbase_va[i])
		    && (addr <= ddp_regbase_va[i] + 0x1000))
			break;
		if ((isVa == 0) && (addr >= ddp_regbase_pa[i])
		    && (addr <= ddp_regbase_pa[i] + 0x1000))
			break;
	}

	if (i < DISP_MODULE_NUM) {
		DDPMSG("addr valid, isVa=0x%x, addr=0x%lx, module=%s!\n", isVa, addr,
		       dpmgr_module_name_spy(i));
		return 1;
	}
	DDPERR("is_reg_addr_valid return fail, isVa=0x%x, addr=0x%lx!\n", isVa, addr);
	return 0;

}

static void dbg_opt_ext(const char *opt, char *buf)
{
	char *p = (char *)opt + 6;
	unsigned int enable = 0;
	int ret;

	ret = kstrtouint(p, 0, &enable);
	if (ret) {
		pr_err("DISP/%s: errno %d\n", __func__, ret);
		goto Error;
	}

	if (enable == 1) {
		DDPMSG("[DDP] debug=1, trigger AEE\n");
		/*aee_kernel_exception("DDP-TEST-ASSERT", "[DDP] DDP-TEST-ASSERT"); *//*kernel 3.18*/
	} else if (enable == 2)
		ddp_mem_test();
	else if (enable == 3)
		ddp_lcd_test();
	else if (enable == 4) {
		DDPAEE("test enable=%d\n", enable);
		sprintf(buf, "test enable=%d\n", enable);
	} else if (enable == 6) {
		unsigned int i = 0;
		int *modules = ddp_get_scenario_list(DDP_SCENARIO_PRIMARY_DISP);
		int module_num = ddp_get_module_num(DDP_SCENARIO_PRIMARY_DISP);

		DDPMSG("dump path status:");
		for (i = 0; i < module_num; i++)
			DDPMSG("%s-", ddp_get_module_name(modules[i]));
		DDPMSG("\n");

		ddp_dump_analysis(DISP_MODULE_CONFIG);
		ddp_dump_analysis(DISP_MODULE_MUTEX);
		for (i = 0; i < module_num; i++)
			ddp_dump_analysis(modules[i]);

		if (primary_display_is_decouple_mode()) {
			ddp_dump_analysis(DISP_MODULE_OVL0);
			ddp_dump_analysis(DISP_MODULE_OVL1);
			ddp_dump_analysis(DISP_MODULE_WDMA0);
		}

		ddp_dump_reg(DISP_MODULE_CONFIG);
		ddp_dump_reg(DISP_MODULE_MUTEX);

		if (primary_display_is_decouple_mode()) {
			ddp_dump_reg(DISP_MODULE_OVL0);
			ddp_dump_reg(DISP_MODULE_OVL1);
			ddp_dump_reg(DISP_MODULE_WDMA0);
		}

		for (i = 0; i < module_num; i++)
			ddp_dump_reg(modules[i]);

	} else if (enable == 7) {
		if (dbg_log_level < 3)
			dbg_log_level++;
		else
			dbg_log_level = 0;

		DDPMSG("DDP: dbg_log_level=%d\n", dbg_log_level);
		sprintf(buf, "dbg_log_level: %d\n", dbg_log_level);
	} else if (enable == 9) {
		gOVLBackground = 0xFF0000FF;
		DDPMSG("DDP: gOVLBackground=%d\n", gOVLBackground);
		sprintf(buf, "gOVLBackground: %d\n", gOVLBackground);
	} else if (enable == 10) {
		gOVLBackground = 0xFF000000;
		DDPMSG("DDP: gOVLBackground=%d\n", gOVLBackground);
		sprintf(buf, "gOVLBackground: %d\n", gOVLBackground);
	} else if (enable == 11) {
		unsigned int i = 0;

		for (i = 0; i < DISP_MODULE_NUM; i++) {
			DDPDUMP("i=%d, module=%s, va=0x%lx, pa=0x%x, irq(%d map to %d)\n",
				i, dpmgr_module_name_spy(i), ddp_regbase_va[i],
				ddp_regbase_pa[i], ddp_irq[i], ddp_irq_map[i]);
		}
	} else if (enable == 12) {
		if (gUltraEnable == 0)
			gUltraEnable = 1;
		else
			gUltraEnable = 0;

		DDPMSG("DDP: gUltraEnable=%d\n", gUltraEnable);
		sprintf(buf, "gUltraEnable: %d\n", gUltraEnable);
	} else if (enable == 15) {
		/* extern smi_dumpDebugMsg(void); */
		ddp_dump_analysis(DISP_MODULE_CONFIG);
		ddp_dump_analysis(DISP_MODULE_RDMA0);
		ddp_dump_analysis(DISP_MODULE_OVL0);
		ddp_dump_analysis(DISP_MODULE_OVL1);

		/* dump ultra/preultra related regs */
		DDPMSG("wdma_con1(2c)=0x%x, wdma_con2(0x38)=0x%x, rdma_gmc0(30)=0x%x\n",
		       DISP_REG_GET(DISP_REG_WDMA_BUF_CON1),
		       DISP_REG_GET(DISP_REG_WDMA_BUF_CON2),
		       DISP_REG_GET(DISP_REG_RDMA_MEM_GMC_SETTING_0));
		DDPMSG("rdma_gmc1(38)=0x%x, fifo_con(40)=0x%x\n",
		       DISP_REG_GET(DISP_REG_RDMA_MEM_GMC_SETTING_1),
		       DISP_REG_GET(DISP_REG_RDMA_FIFO_CON));
		DDPMSG("ovl0_gmc: 0x%x, 0x%x, 0x%x, 0x%x, ovl1_gmc: 0x%x, 0x%x, 0x%x, 0x%x\n",
		       DISP_REG_GET(DISP_REG_OVL_RDMA0_MEM_GMC_S2),
		       DISP_REG_GET(DISP_REG_OVL_RDMA1_MEM_GMC_S2),
		       DISP_REG_GET(DISP_REG_OVL_RDMA2_MEM_GMC_S2),
		       DISP_REG_GET(DISP_REG_OVL_RDMA3_MEM_GMC_S2),
		       DISP_REG_GET(DISP_REG_OVL_RDMA0_MEM_GMC_S2 + DISP_OVL_INDEX_OFFSET),
		       DISP_REG_GET(DISP_REG_OVL_RDMA1_MEM_GMC_S2 + DISP_OVL_INDEX_OFFSET),
		       DISP_REG_GET(DISP_REG_OVL_RDMA2_MEM_GMC_S2 + DISP_OVL_INDEX_OFFSET),
		       DISP_REG_GET(DISP_REG_OVL_RDMA3_MEM_GMC_S2 + DISP_OVL_INDEX_OFFSET));

		/* dump smi regs */
		/* smi_dumpDebugMsg(); */

	} else if (enable == 16) {
		if (gDumpMemoutCmdq == 0)
			gDumpMemoutCmdq = 1;
		else
			gDumpMemoutCmdq = 0;

		DDPMSG("DDP: gDumpMemoutCmdq=%d\n", gDumpMemoutCmdq);
		sprintf(buf, "gDumpMemoutCmdq: %d\n", gDumpMemoutCmdq);
	} else if (enable == 21) {
		if (gEnableSODIControl == 0)
			gEnableSODIControl = 1;
		else
			gEnableSODIControl = 0;

		DDPMSG("DDP: gEnableSODIControl=%d\n", gEnableSODIControl);
		sprintf(buf, "gEnableSODIControl: %d\n", gEnableSODIControl);
	} else if (enable == 22) {
		if (gPrefetchControl == 0)
			gPrefetchControl = 1;
		else
			gPrefetchControl = 0;

		DDPMSG("DDP: gPrefetchControl=%d\n", gPrefetchControl);
		sprintf(buf, "gPrefetchControl: %d\n", gPrefetchControl);

	} else if (enable == 28) {
		if (gDumpClockStatus == 0)
			gDumpClockStatus = 1;
		else
			gDumpClockStatus = 0;

		DDPMSG("DDP: gDumpClockStatus=%d\n", gDumpClockStatus);
		sprintf(buf, "gDumpClockStatus: %d\n", gDumpClockStatus);

	} else if (enable == 29) {
		if (gEnableUartLog == 0)
			gEnableUartLog = 1;
		else
			gEnableUartLog = 0;

		DDPMSG("DDP: gEnableUartLog=%d\n", gEnableUartLog);
		sprintf(buf, "gEnableUartLog: %d\n", gEnableUartLog);

	} else if (enable == 30) {
		if (gEnableMutexRisingEdge == 0) {
			gEnableMutexRisingEdge = 1;
			DISP_REG_SET_FIELD(0, SOF_FLD_MUTEX0_SOF_TIMING,
					   DISP_REG_CONFIG_MUTEX0_SOF, 1);
		} else {
			gEnableMutexRisingEdge = 0;
			DISP_REG_SET_FIELD(0, SOF_FLD_MUTEX0_SOF_TIMING,
					   DISP_REG_CONFIG_MUTEX0_SOF, 0);
		}

		DDPMSG("DDP: gEnableMutexRisingEdge=%d\n", gEnableMutexRisingEdge);
		sprintf(buf, "gEnableMutexRisingEdge: %d\n", gEnableMutexRisingEdge);

	} else if (enable == 31) {
		if (gEnableReduceRegWrite == 0)
			gEnableReduceRegWrite = 1;
		else
			gEnableReduceRegWrite = 0;

		DDPMSG("DDP: gEnableReduceRegWrite=%d\n", gEnableReduceRegWrite);
		sprintf(buf, "gEnableReduceRegWrite: %d\n", gEnableReduceRegWrite);

	} else if (enable == 32) {
		DDPAEE("DDP: (32)gEnableReduceRegWrite=%d\n", gEnableReduceRegWrite);
	} else if (enable == 33) {
		if (gDumpConfigCMD == 0)
			gDumpConfigCMD = 1;
		else
			gDumpConfigCMD = 0;

		DDPMSG("DDP: gDumpConfigCMD=%d\n", gDumpConfigCMD);
		sprintf(buf, "gDumpConfigCMD: %d\n", gDumpConfigCMD);

	} else if (enable == 34) {
		if (gESDEnableSODI == 0)
			gESDEnableSODI = 1;
		else
			gESDEnableSODI = 0;

		DDPMSG("DDP: gESDEnableSODI=%d\n", gESDEnableSODI);
		sprintf(buf, "gESDEnableSODI: %d\n", gESDEnableSODI);

	} else if (enable == 35) {
		if (gEnableOVLStatusCheck == 0)
			gEnableOVLStatusCheck = 1;
		else
			gEnableOVLStatusCheck = 0;

		DDPMSG("DDP: gEnableOVLStatusCheck=%d\n", gEnableOVLStatusCheck);
		sprintf(buf, "gEnableOVLStatusCheck: %d\n", gEnableOVLStatusCheck);

	} else if (enable == 36) {
		if (gResetRDMAEnable == 0)
			gResetRDMAEnable = 1;
		else
			gResetRDMAEnable = 0;

		DDPMSG("DDP: gResetRDMAEnable=%d\n", gResetRDMAEnable);
		sprintf(buf, "gResetRDMAEnable: %d\n", gResetRDMAEnable);
	} else if (enable == 37) {
		unsigned int reg_value = 0;

		if (gEnableIRQ == 0) {
			gEnableIRQ = 1;

			DISP_CPU_REG_SET(DISP_REG_OVL_INTEN, 0x1e2);
			DISP_CPU_REG_SET(DISP_REG_OVL_INTEN + DISP_OVL_INDEX_OFFSET, 0x1e2);

			reg_value = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTEN);
			DISP_CPU_REG_SET(DISP_REG_CONFIG_MUTEX_INTEN,
					 reg_value | (1 << 0) | (1 << DISP_MUTEX_TOTAL));
		} else {
			gEnableIRQ = 0;

			DISP_CPU_REG_SET(DISP_REG_OVL_INTEN, 0x1e0);
			DISP_CPU_REG_SET(DISP_REG_OVL_INTEN + DISP_OVL_INDEX_OFFSET, 0x1e0);

			reg_value = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTEN);
			DISP_CPU_REG_SET(DISP_REG_CONFIG_MUTEX_INTEN,
					 reg_value & (~(1 << 0)) & (~(1 << DISP_MUTEX_TOTAL)));

		}

		DDPMSG("DDP: gEnableIRQ=%d\n", gEnableIRQ);
		sprintf(buf, "gEnableIRQ: %d\n", gEnableIRQ);

	} else if (enable == 38) {
		if (gDisableSODIForTriggerLoop == 0)
			gDisableSODIForTriggerLoop = 1;
		else
			gDisableSODIForTriggerLoop = 0;

		DDPMSG("DDP: gDisableSODIForTriggerLoop=%d\n", gDisableSODIForTriggerLoop);
		sprintf(buf, "gDisableSODIForTriggerLoop: %d\n", gDisableSODIForTriggerLoop);

	} else if (enable == 39) {
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_STREAM_EOF);
		cmdqCoreSetEvent(CMDQ_EVENT_DISP_RDMA0_EOF);
		sprintf(buf, "enable=%d\n", enable);
	} else if (enable == 40) {
		/*sprintf(buf, "version: %d, %s\n", 7, __TIME__); */ /*kernel 3.18*/
	} else if (enable == 41) {
		if (gResetOVLInAALTrigger == 0)
			gResetOVLInAALTrigger = 1;
		else
			gResetOVLInAALTrigger = 0;

		DDPMSG("DDP: gResetOVLInAALTrigger=%d\n", gResetOVLInAALTrigger);
		sprintf(buf, "gResetOVLInAALTrigger: %d\n", gResetOVLInAALTrigger);

	} else if (enable == 42) {
		if (gDisableOVLTF == 0)
			gDisableOVLTF = 1;
		else
			gDisableOVLTF = 0;

		DDPMSG("DDP: gDisableOVLTF=%d\n", gDisableOVLTF);
		sprintf(buf, "gDisableOVLTF: %d\n", gDisableOVLTF);

	} else if (enable == 43) {
		if (gDumpESDCMD == 0)
			gDumpESDCMD = 1;
		else
			gDumpESDCMD = 0;

		DDPMSG("DDP: gDumpESDCMD=%d\n", gDumpESDCMD);
		sprintf(buf, "gDumpESDCMD: %d\n", gDumpESDCMD);

	} else if (enable == 44) {
		disp_dump_emi_status();
		sprintf(buf, "dump emi status!\n");
	} else if (enable == 50) {
		set_ddp_log_level(8);
	}
Error:
	return;
}

static void process_dbg_opt(const char *opt)
{
	char *buf = dbg_buf + strlen(dbg_buf);
	int ret = 0;

	if (0 == strncmp(opt, "regr:", 5)) {
		char *p = (char *)opt + 5;
		unsigned long addr = 0;

		ret = kstrtoul(p, 16, (unsigned long int *)&addr);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}
		if (is_reg_addr_valid(1, addr) == 1) {
			unsigned int regVal = DISP_REG_GET(addr);

			DDPMSG("regr: 0x%lx = 0x%08x\n", addr, regVal);
			sprintf(buf, "regr: 0x%lx = 0x%08x\n", addr, regVal);
		} else {
			sprintf(buf, "regr, invalid address 0x%lx\n", addr);
			goto Error;
		}
	} else if (0 == strncmp(opt, "regw:", 5)) {
		char *p = (char *)opt + 5;
		unsigned long addr = 0;
		unsigned long val = 0;
		char *tmp;

		tmp = strsep(&p, ",");
		ret = kstrtoul(tmp, 16, (unsigned long int *)&addr);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}
		tmp = strsep(&p, " ");
		ret = kstrtoul(tmp, 16, (unsigned long int *)&val);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}

		if (is_reg_addr_valid(1, addr) == 1) {
			unsigned int regVal;

			DISP_CPU_REG_SET(addr, val);
			regVal = DISP_REG_GET(addr);
			DDPMSG("regw: 0x%lx, 0x%08lx = 0x%08x\n", addr, val, regVal);
			sprintf(buf, "regw: 0x%lx, 0x%08lx = 0x%08x\n", addr, val, regVal);
		} else {
			sprintf(buf, "regw, invalid address 0x%lx\n", addr);
			goto Error;
		}
	} else if (0 == strncmp(opt, "regr_pa:", 8)) {
		char *p = (char *)opt + 8;
		unsigned long addr_pa = 0;
		unsigned long addr_va = 0;

		ret = kstrtoul(p, 16, (unsigned long int *)&addr_pa);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}
		DDPMSG("p: %s\n", p);
		addr_va = ddp_addr_convert_pa2va(addr_pa);
		if (addr_va != 0) {
			unsigned int regVal = DISP_REG_GET(addr_va);

			DDPMSG("regr_pa: 0x%08lx = 0x%08x\n", addr_pa, regVal);
			sprintf(buf, "regr_pa: 0x%08lx = 0x%08x\n", addr_pa, regVal);
		} else {
			sprintf(buf, "regr_pa, invalid pa address 0x%08lx\n", addr_pa);
			goto Error;
		}
	} else if (0 == strncmp(opt, "regw_pa:", 8)) {
		char *p = (char *)opt + 8;
		unsigned long addr_pa = 0;
		unsigned long addr_va = 0;
		unsigned long val = 0;
		char *tmp;

		tmp = strsep(&p, ",");
		ret = kstrtoul(tmp, 16, (unsigned long int *)&addr_pa);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}
		tmp = strsep(&p, " ");
		ret = kstrtoul(tmp, 16, (unsigned long int *)&val);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}

		addr_va = ddp_addr_convert_pa2va(addr_pa);
		if (addr_va != 0) {
			unsigned int regVal;

			DISP_CPU_REG_SET(addr_va, val);
			regVal = DISP_REG_GET(addr_va);
			DDPMSG("regw_pa: 0x%08lx, 0x%08lx = 0x%08x\n", addr_pa, val, regVal);
			sprintf(buf, "regw_pa: 0x%08lx, 0x%08lx = 0x%08x\n", addr_pa, val, regVal);
		} else {
			sprintf(buf, "regw_pa, invalid address 0x%08lx\n", addr_pa);
			goto Error;
		}
	} else if (0 == strncmp(opt, "rdma_ultra:", 11)) {
		char *p = (char *)opt + 11;

		ret = kstrtouint(p, 0, &gRDMAUltraSetting);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}
		DISP_CPU_REG_SET(DISP_REG_RDMA_MEM_GMC_SETTING_0, gRDMAUltraSetting);
		sprintf(buf, "rdma_ultra, gRDMAUltraSetting=0x%x, reg=0x%x\n", gRDMAUltraSetting,
			DISP_REG_GET(DISP_REG_RDMA_MEM_GMC_SETTING_0));
	} else if (0 == strncmp(opt, "rdma_fifo:", 10)) {
		char *p = (char *)opt + 10;

		ret = kstrtouint(p, 0, &gRDMAFIFOLen);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}

		DISP_CPU_REG_SET_FIELD(FIFO_CON_FLD_OUTPUT_VALID_FIFO_THRESHOLD,
				       DISP_REG_RDMA_FIFO_CON, gRDMAFIFOLen);
		sprintf(buf, "rdma_fifo, gRDMAFIFOLen=0x%x, reg=0x%x\n", gRDMAFIFOLen,
			DISP_REG_GET(DISP_REG_RDMA_FIFO_CON));
	} else if (0 == strncmp(opt, "g_regr:", 7)) {
		char *p = (char *)opt + 7;
		unsigned int reg_va_before = 0;
		unsigned long reg_va = 0;
		unsigned long reg_pa = 0;

		ret = kstrtoul(p, 16, (unsigned long int *)&reg_pa);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}
		if (reg_pa < 0x10000000 || reg_pa > 0x18000000) {
			sprintf(buf, "g_regr, invalid pa=0x%lx\n", reg_pa);
		} else {
			reg_va = (unsigned long)ioremap_nocache(reg_pa, sizeof(unsigned long));
			reg_va_before = DISP_REG_GET(reg_va);
			DDPMSG("g_regr, pa=%lx, va=0x%lx, reg_val=0x%x\n",
			       reg_pa, reg_va, reg_va_before);
			sprintf(buf, "g_regr, pa=%lx, va=0x%lx, reg_val=0x%x\n",
				reg_pa, reg_va, reg_va_before);

			iounmap((void *)reg_va);
		}
	} else if (0 == strncmp(opt, "g_regw:", 7)) {
		char *p = (char *)opt + 7;
		unsigned int reg_va_before = 0;
		unsigned int reg_va_after = 0;
		unsigned int val = 0;
		unsigned long reg_va = 0;
		unsigned long reg_pa = 0;
		char *tmp;

		p = (char *)opt + 7;
		tmp = strsep(&p, ",");
		ret = kstrtoul(tmp, 16, (unsigned long int *)&reg_pa);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}

		if (reg_pa < 0x10000000 || reg_pa > 0x18000000) {
			sprintf(buf, "g_regw, invalid pa=0x%lx\n", reg_pa);
		} else {
			tmp = strsep(&p, " ");
			ret = kstrtouint(tmp, 0, &val);
			reg_va = (unsigned long)ioremap_nocache(reg_pa, sizeof(unsigned long));
			reg_va_before = DISP_REG_GET(reg_va);
			DISP_CPU_REG_SET(reg_va, val);
			reg_va_after = DISP_REG_GET(reg_va);

			DDPMSG
			    ("g_regw, pa=%lx, va=0x%lx, value=0x%x, reg_val_before=0x%x, reg_val_after=0x%x\n",
			     reg_pa, reg_va, val, reg_va_before, reg_va_after);
			sprintf(buf,
				"g_regw, pa=%lx, va=0x%lx, value=0x%x, reg_val_before=0x%x, reg_val_after=0x%x\n",
				reg_pa, reg_va, val, reg_va_before, reg_va_after);

			iounmap((void *)reg_va);
		}
	} else if (0 == strncmp(opt, "dbg_log:", 8)) {
		char *p = (char *)opt + 8;
		unsigned int enable = 0;

		ret = kstrtouint(p, 0, &enable);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}
		if (enable)
			dbg_log_level = 1;
		else
			dbg_log_level = 0;

		sprintf(buf, "dbg_log: %d\n", dbg_log_level);
	} else if (0 == strncmp(opt, "irq_log:", 8)) {
		char *p = (char *)opt + 8;
		unsigned int enable = 0;

		ret = kstrtouint(p, 0, &enable);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}
		if (enable)
			irq_log_level = 1;
		else
			irq_log_level = 0;

		sprintf(buf, "irq_log: %d\n", irq_log_level);
	} else if (0 == strncmp(opt, "met_on:", 7)) {
		char *p = (char *)opt + 7;
		unsigned int met_on = 0;
		unsigned int rdma0_mode = 0;
		unsigned int rdma1_mode = 0;
		char *tmp;

		tmp = strsep(&p, ",");
		ret = kstrtouint(tmp, 0, &met_on);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}
		tmp = strsep(&p, ",");
		ret = kstrtouint(tmp, 0, &rdma0_mode);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}
		tmp = strsep(&p, " ");
		ret = kstrtouint(tmp, 0, &rdma1_mode);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}
		ddp_init_met_tag(met_on, rdma0_mode, rdma1_mode);
		DDPMSG("process_dbg_opt, met_on=%d,rdma0_mode %d, rdma1 %d\n", met_on, rdma0_mode,
		       rdma1_mode);
		sprintf(buf, "met_on:%d,rdma0_mode:%d,rdma1_mode:%d\n", met_on, rdma0_mode,
			rdma1_mode);
	} else if (0 == strncmp(opt, "backlight:", 10)) {
		char *p = (char *)opt + 10;
		unsigned int level = 0;

		ret = kstrtouint(p, 0, &level);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}
		if (level) {
			disp_bls_set_backlight(level);
			sprintf(buf, "backlight: %d\n", level);
		} else {
			goto Error;
		}
	} else if (0 == strncmp(opt, "pwm0:", 5) || 0 == strncmp(opt, "pwm1:", 5)) {
		char *p = (char *)opt + 5;
		unsigned int level = 0;

		ret = kstrtouint(p, 0, &level);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}
		if (level) {
			disp_pwm_id_t pwm_id = DISP_PWM0;

			if (opt[3] == '1')
				pwm_id = DISP_PWM1;

			disp_pwm_set_backlight(pwm_id, level);
			sprintf(buf, "PWM 0x%x : %d\n", pwm_id, level);
		} else {
			goto Error;
		}
	} else if (0 == strncmp(opt, "aal_dbg:", 8)) {
		char *p = (char *)opt + 8;

		ret = kstrtoint(p, 10, &aal_dbg_en);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}
		sprintf(buf, "aal_dbg_en = 0x%x\n", aal_dbg_en);
	} else if (0 == strncmp(opt, "dump_module:", 12)) {
		char *p = (char *)opt + 12;
		char *dst;
		unsigned int len = strlen(p);
		unsigned int module = 0;

		if (len > 0)
			p[len - 1] = '\0';
		for (module = 0; module < DISP_MODULE_NUM; module++) {
			dst = ddp_get_module_name(module);
			if (strstr(dst, p) != NULL)
				break;
		}
		DDPMSG("process_dbg_opt, module=%d\n", module);
		if (module < DISP_MODULE_NUM) {
			dprec_logger_dump_reset();
			dump_to_buffer = 1;
			ddp_dump_reg(module);
			dump_to_buffer = 0;
			memset(dbg_buf, 0, sizeof(dbg_buf));
			buf = dbg_buf + strlen(dbg_buf);
			sprintf(buf, "dump_module: %d\n", module);
			buf = dbg_buf + strlen(dbg_buf);
			memcpy(buf, dprec_logger_get_dump_addr(), dprec_logger_get_dump_len());
		} else {
			DDPMSG("process_dbg_opt2, module=%d\n", module);
			goto Error;
		}
	} else if (0 == strncmp(opt, "dump_path:", 10)) {
		char *p = (char *)opt + 10;
		unsigned int mutex_idx = 0;

		ret = kstrtouint(p, 0, &mutex_idx);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}
		DDPMSG("process_dbg_opt, path mutex=%d\n", mutex_idx);
		dpmgr_debug_path_status(mutex_idx);
		sprintf(buf, "dump_path: %d\n", mutex_idx);
	} else if (0 == strncmp(opt, "lowpower:", 9)) {
		char *p = (char *)opt + 9;
		unsigned int session = 0;
		unsigned int value = 0;
		char *tmp;

		tmp = strsep(&p, ",");
		ret = kstrtouint(tmp, 0, &session);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}
		tmp = strsep(&p, " ");
		ret = kstrtouint(tmp, 0, &value);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}
		DDPMSG("process_dbg_opt, lowpower: session %d,value %d\n", session, value);

		if (session == 0) {
			if (value == 0)
				disp_low_power_reduse_fps = 0;
			else
				disp_low_power_reduse_fps = 1;

			DDPMSG("DDP: disp_low_power_reduse_fps=%d\n", disp_low_power_reduse_fps);
			sprintf(buf, "disp_low_power_reduse_fps: %d\n", disp_low_power_reduse_fps);

		} else if (session == 1) {
			if (value == 0)
				disp_low_power_reduse_clock = 0;
			else
				disp_low_power_reduse_clock = value;

			DDPMSG("DDP: disp_low_power_reduse_clock=%d\n",
			       disp_low_power_reduse_clock);
			sprintf(buf, "disp_low_power_reduse_clock: %d\n",
				disp_low_power_reduse_clock);

		} else if (session == 2) {
			if (value == 0)
				disp_low_power_adjust_vfp = 0;
			else
				disp_low_power_adjust_vfp = value;

			DDPMSG("DDP: disp_low_power_adjust_vfp=%d\n", disp_low_power_adjust_vfp);
			sprintf(buf, "disp_low_power_adjust_vfp: %d\n", disp_low_power_adjust_vfp);

		} else if (session == 3) {
			if (value == 0)
				disp_low_power_disable_ddp_clock = 0;
			else
				disp_low_power_disable_ddp_clock = 1;

			DDPMSG("DDP: disp_low_power_disable_ddp_clock=%d\n",
			       disp_low_power_disable_ddp_clock);
			sprintf(buf, "disp_low_power_disable_ddp_clock: %d\n",
				disp_low_power_disable_ddp_clock);

		} else if (session == 4) {
			if (value == 0)
				disp_low_power_remove_ovl = 0;
			else
				disp_low_power_remove_ovl = 1;

			DDPMSG("DDP: disp_low_power_remove_ovl=%d\n", disp_low_power_remove_ovl);
			sprintf(buf, "disp_low_power_remove_ovl: %d\n", disp_low_power_remove_ovl);

		} else if (session == 5) {
			if (value == 0)
				gSkipIdleDetect = 0;
			else
				gSkipIdleDetect = 1;

			DDPMSG("DDP: gSkipIdleDetect=%d\n", gSkipIdleDetect);
			sprintf(buf, "gSkipIdleDetect: %d\n", gSkipIdleDetect);
		}
	} else if (0 == strncmp(opt, "debug:", 6)) {
		dbg_opt_ext(opt, buf);
	} else if (0 == strncmp(opt, "mmp", 3)) {
		init_ddp_mmp_events();
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	} else if (0 == strncmp(opt, "svp:", 4)) {
		char *p = (char *)opt + 4;
		unsigned int session = 0;
		unsigned int option = 0;
		char *tmp;

		tmp = strsep(&p, ",");
		ret = kstrtouint(tmp, 0, &session);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}
		tmp = strsep(&p, " ");
		ret = kstrtouint(tmp, 0, &option);
		if (ret) {
			DDPERR("DISP/%s: line:%d errno %d\n", __func__, __LINE__, ret);
			goto Error;
		}

		gDebugSvp = session;
		gDebugSvpOption = option;
		DDPMSG("DDP: gDebugSvp=%d, gDebugSvpOption=%d\n", gDebugSvp, gDebugSvpOption);
		if (session == 0) {
			DDPMSG("switch to normal world\n");
		} else if (session == 1) {
			DDPMSG("replace sec handle with internal buffer handle\n");
		} else if (session == 2) {
			DDPMSG("switch to secure world\n");
		}
#endif
	} else {
		dbg_buf[0] = '\0';
		goto Error;
	}

	return;

Error:
	DDPERR("parse command error!\n%s\n\n%s", opt, STR_HELP);
}


static void process_dbg_cmd(char *cmd)
{
	char *tok;

	DDPDBG("cmd: %s\n", cmd);
	memset(dbg_buf, 0, sizeof(dbg_buf));
	while ((tok = strsep(&cmd, "&&")) != NULL) {
		DDPMSG("parse: %s\n", tok);
		process_dbg_opt(tok);
	}
}


/* --------------------------------------------------------------------------- */
/* Debug FileSystem Routines */
/* --------------------------------------------------------------------------- */

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}


static char cmd_buf[512];

static ssize_t debug_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
	if (strlen(dbg_buf))
		return simple_read_from_buffer(ubuf, count, ppos, dbg_buf, strlen(dbg_buf));
	else
		return simple_read_from_buffer(ubuf, count, ppos, STR_HELP, strlen(STR_HELP));

}


static ssize_t debug_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(cmd_buf) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&cmd_buf, ubuf, count))
		return -EFAULT;

	cmd_buf[count] = 0;

	process_dbg_cmd(cmd_buf);

	return ret;
}


static const struct file_operations debug_fops = {
	.read = debug_read,
	.write = debug_write,
	.open = debug_open,
};

static ssize_t debug_dump_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{

	dprec_logger_dump_reset();
	dump_to_buffer = 1;
	/* dump all */
	dpmgr_debug_path_status(-1);
	dump_to_buffer = 0;
	return simple_read_from_buffer(buf, size, ppos, dprec_logger_get_dump_addr(),
				       dprec_logger_get_dump_len());
}


static const struct file_operations debug_fops_dump = {
	.read = debug_dump_read,
};

void ddp_debug_init(void)
{
	if (!debug_init) {
		debug_init = 1;
		debugfs = debugfs_create_file("dispsys",
					      S_IFREG | S_IRUGO, NULL, (void *)0, &debug_fops);


		debugDir = debugfs_create_dir("disp", NULL);
		if (debugDir) {

			debugfs_dump = debugfs_create_file("dump",
							   S_IFREG | S_IRUGO, debugDir, NULL,
							   &debug_fops_dump);
		}
	}
}

unsigned int ddp_debug_analysis_to_buffer(void)
{
	return dump_to_buffer;
}

unsigned int ddp_debug_dbg_log_level(void)
{
	return dbg_log_level;
}

unsigned int ddp_debug_irq_log_level(void)
{
	return irq_log_level;
}


void ddp_debug_exit(void)
{
	debugfs_remove(debugfs);
	debugfs_remove(debugfs_dump);
	debug_init = 0;
}

int ddp_mem_test(void)
{
	return -1;
}

int ddp_lcd_test(void)
{
	return -1;
}

char *disp_get_fmt_name(DP_COLOR_ENUM color)
{
	switch (color) {
	case DP_COLOR_FULLG8:
		return "fullg8";
	case DP_COLOR_FULLG10:
		return "fullg10";
	case DP_COLOR_FULLG12:
		return "fullg12";
	case DP_COLOR_FULLG14:
		return "fullg14";
	case DP_COLOR_UFO10:
		return "ufo10";
	case DP_COLOR_BAYER8:
		return "bayer8";
	case DP_COLOR_BAYER10:
		return "bayer10";
	case DP_COLOR_BAYER12:
		return "bayer12";
	case DP_COLOR_RGB565:
		return "rgb565";
	case DP_COLOR_BGR565:
		return "bgr565";
	case DP_COLOR_RGB888:
		return "rgb888";
	case DP_COLOR_BGR888:
		return "bgr888";
	case DP_COLOR_RGBA8888:
		return "rgba";
	case DP_COLOR_BGRA8888:
		return "bgra";
	case DP_COLOR_ARGB8888:
		return "argb";
	case DP_COLOR_ABGR8888:
		return "abgr";
	case DP_COLOR_I420:
		return "i420";
	case DP_COLOR_YV12:
		return "yv12";
	case DP_COLOR_NV12:
		return "nv12";
	case DP_COLOR_NV21:
		return "nv21";
	case DP_COLOR_I422:
		return "i422";
	case DP_COLOR_YV16:
		return "yv16";
	case DP_COLOR_NV16:
		return "nv16";
	case DP_COLOR_NV61:
		return "nv61";
	case DP_COLOR_YUYV:
		return "yuyv";
	case DP_COLOR_YVYU:
		return "yvyu";
	case DP_COLOR_UYVY:
		return "uyvy";
	case DP_COLOR_VYUY:
		return "vyuy";
	case DP_COLOR_I444:
		return "i444";
	case DP_COLOR_YV24:
		return "yv24";
	case DP_COLOR_IYU2:
		return "iyu2";
	case DP_COLOR_NV24:
		return "nv24";
	case DP_COLOR_NV42:
		return "nv42";
	case DP_COLOR_GREY:
		return "grey";
	default:
		return "undefined";
	}

}

unsigned int ddp_dump_reg_to_buf(unsigned int start_module, unsigned long *addr)
{
	unsigned int cnt = 0;
	unsigned long reg_addr;

	switch (start_module) {
	case 0:		/* DISP_MODULE_WDMA0: */
		reg_addr = DISP_REG_WDMA_INTEN;

		while (reg_addr <= DISP_REG_WDMA_PRE_ADD2) {
			addr[cnt++] = DISP_REG_GET(reg_addr);
			reg_addr += 4;
		}
		break;
	case 1:		/* DISP_MODULE_OVL: */
		reg_addr = DISP_REG_OVL_STA;

		while (reg_addr <= DISP_REG_OVL_L3_PITCH) {
			addr[cnt++] = DISP_REG_GET(reg_addr);
			reg_addr += 4;
		}
		break;
	case 2:		/* DISP_MODULE_RDMA: */
		reg_addr = DISP_REG_RDMA_INT_ENABLE;

		while (reg_addr <= DISP_REG_RDMA_PRE_ADD_1) {
			addr[cnt++] = DISP_REG_GET(reg_addr);
			reg_addr += 4;
		}
		break;
	}
	return cnt * sizeof(unsigned long);
}
