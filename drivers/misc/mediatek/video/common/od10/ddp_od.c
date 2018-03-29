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

#include <linux/gfp.h>
#include <linux/types.h>
#include <linux/string.h> /* for test cases */
#include <asm/cacheflush.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/mutex.h>

/* #include <mach/mt_spm_idle.h> */
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#else
#include <ddp_clkmgr.h>
#endif
#include <m4u.h>
#include <ddp_drv.h>
#include <ddp_reg.h>
#include <lcm_drv.h>
#include <ddp_dither.h>
#include <ddp_od.h>
#include <ddp_path.h>
#include <ddp_dump.h>
#include "ddp_od_reg.h"
#include "ddp_od_table.h"

#if defined(COMMON_DISP_LOG)
#include <disp_debug.h>
#include <disp_log.h>
#else
#include <disp_drv_log.h>
#include <ddp_log.h>
#endif

#define OD_ALLOW_DEFAULT_TABLE
/* #define OD_LINEAR_TABLE_IF_NONE */

/* compression ratio */
#define OD_MANUAL_CPR   58 /* 38 */
#define OD_HSYNC_WIDTH  100
#define OD_LINESIZE_BUFFER 8

/* Additional space after the given memory to avoid unexpected memory corruption */
/* In the experiments, OD hardware overflew 240 bytes */
#define OD_ADDITIONAL_BUFFER 256

#define OD_GUARD_PATTERN 0x16881688
#define OD_GUARD_PATTERN_SIZE 4

#define OD_REG_SET_FIELD(cmdq, reg, val, field) DISP_REG_SET_FIELD(cmdq, field, (unsigned long)(reg), val)
#define OD_REG_GET(reg32) DISP_REG_GET((unsigned long)(reg32))
#define OD_REG_SET(handle, reg32, val) DISP_REG_SET(handle, (unsigned long)(reg32), val)

#define ABS(a) ((a > 0) ? a : -a)

/* debug macro */
#define ODERR(fmt, arg...) pr_err("[OD] " fmt "\n", ##arg)
#define ODNOTICE(fmt, arg...) pr_warn("[OD] " fmt "\n", ##arg)


/* ioctl */
typedef enum {
	OD_CTL_READ_REG,
	OD_CTL_WRITE_REG,
	OD_CTL_ENABLE_DEMO_MODE,
	OD_CTL_RUN_TEST,
	OD_CTL_WRITE_TABLE,
	OD_CTL_CMD_NUM,
	OD_CTL_ENABLE
} DISP_OD_CMD_TYPE;

typedef enum {
	OD_CTL_ENABLE_OFF,
	OD_CTL_ENABLE_ON
} DISP_OD_ENABLE_STAGE;

enum {
	OD_RED,
	OD_GREEN,
	OD_BLUE,
	OD_ALL
};

enum {
	OD_TABLE_17,
	OD_TABLE_33
};


struct OD_BUFFER_STRUCT {
	unsigned int size;
	unsigned long pa;
	void *va;
} g_od_buf;


enum OD_LOG_LEVEL {
	OD_LOG_ALWAYS = 0,
	OD_LOG_VERBOSE,
	OD_LOG_DEBUG
};
static int od_log_level = -1;
#define ODDBG(level, fmt, arg...) \
	do { \
		if (od_log_level >= (level)) \
			pr_debug("[OD] " fmt "\n", ##arg); \
	} while (0)

static int g_od_is_demo_mode;

enum OD_DEBUG_MODE {
	DEBUG_MODE_NONE = 0,
	DEBUG_MODE_OSD = 0x1,
	DEBUG_MODE_INK = 0x2,
	DEBUG_MODE_INK_OSD = (DEBUG_MODE_OSD | DEBUG_MODE_INK)
};
static unsigned int g_od_debug_mode = DEBUG_MODE_NONE;

static DEFINE_MUTEX(g_od_global_lock);
static volatile int g_od_is_enabled; /* OD is disabled by default */

enum OD_DISABLED_BIT_FLAGS {
	DISABLED_BY_HWC = 0,
	DISABLED_BY_MHL
};
static volatile unsigned long g_od_force_disabled; /* Initialized to 0 */

enum OD_DEBUG_ENABLE_ENUM {
	DEBUG_ENABLE_NORMAL = 0,
	DEBUG_ENABLE_ALWAYS,
	DEBUG_ENABLE_NEVER
};
static enum OD_DEBUG_ENABLE_ENUM g_od_debug_enable = DEBUG_ENABLE_NORMAL;

static volatile struct {
	unsigned int frame;
	unsigned int rdma_error;
	unsigned int rdma_normal;
} g_od_debug_cnt = { 0 };

static void od_dump_all(void);
static ddp_module_notify g_od_ddp_notify;

#if defined(CONFIG_MTK_OD_SUPPORT)

static int od_start(DISP_MODULE_ENUM module, void *cmdq);

static void _od_reg_init(void *cmdq)
{
	DISP_REG_SET(cmdq, OD_REG00, 0x82040201);
	DISP_REG_SET(cmdq, OD_REG01, 0x00070E01 & 0x000000ff);
	DISP_REG_SET(cmdq, OD_REG02, 0x00000400);
	DISP_REG_SET(cmdq, OD_REG03, 0x00E0FF04);

	DISP_REG_SET(cmdq, OD_REG30, 0x00010026); /* set cr rate ==0x42 */
	DISP_REG_SET(cmdq, OD_REG33, 0x00021000 & 0x003fffff);
	DISP_REG_SET(cmdq, OD_REG34, 0x0674A0E6);
	DISP_REG_SET(cmdq, OD_REG35, 0x000622E0); /* 0x00051170 */
	DISP_REG_SET(cmdq, OD_REG36, 0xE0849C40); /* DET8B_POS=4 */
	DISP_REG_SET(cmdq, OD_REG37, 0x05CFDE08); /* 0x04E6D998 */
	DISP_REG_SET(cmdq, OD_REG38, 0x011F1013); /* line size=0xf8 | (0x37 << 13) */
	DISP_REG_SET(cmdq, OD_REG39, 0x00200000 & 0x00ff0000); /* dram_crc_cnt=0x20 */
	DISP_REG_SET(cmdq, OD_REG40, 0x20000610); /* enable GM */
	DISP_REG_SET(cmdq, OD_REG41, 0x001E02D0);
	DISP_REG_SET(cmdq, OD_REG42, 0x00327C7C);
	DISP_REG_SET(cmdq, OD_REG43, 0x00180000); /* disable film mode detection */
	DISP_REG_SET(cmdq, OD_REG44, 0x006400C8);
	DISP_REG_SET(cmdq, OD_REG45, 0x00210032 & 0xfffffffc); /* pcid_alig_sel=1 */
	DISP_REG_SET(cmdq, OD_REG46, 0x4E00023F);
	DISP_REG_SET(cmdq, OD_REG47, 0xC306B16A); /* pre_bw=3 */
	DISP_REG_SET(cmdq, OD_REG48, 0x10200408);
	DISP_REG_SET(cmdq, OD_REG49, 0xC5C00000 & 0xC7C00000);
	DISP_REG_SET(cmdq, OD_REG51, 0x21A1B800);/* dump burst */
	DISP_REG_SET(cmdq, OD_REG52, 0x60000044 & 0xe00000ff);/* DUMP_WFF_FULL_CONF=3 */
	DISP_REG_SET(cmdq, OD_REG53, 0x2FFF3E00);
	DISP_REG_SET(cmdq, OD_REG54, 0x80039000); /* new 3x3 */
	DISP_REG_SET(cmdq, OD_REG57, 0x352835CA); /* skip color thr  */
	DISP_REG_SET(cmdq, OD_REG62, 0x00014438); /* pattern gen */
	DISP_REG_SET(cmdq, OD_REG63, 0x27800898);
	DISP_REG_SET(cmdq, OD_REG64, 0x00438465);
	DISP_REG_SET(cmdq, OD_REG65, 0x01180001);
	DISP_REG_SET(cmdq, OD_REG66, 0x0002D000);
	DISP_REG_SET(cmdq, OD_REG67, 0x3FFFFFFF);
	DISP_REG_SET(cmdq, OD_REG68, 0x00000000);
	DISP_REG_SET(cmdq, OD_REG69, 0x000200C0);
}

#endif /* defined(CONFIG_MTK_OD_SUPPORT) */

static void od_refresh_screen(void)
{
#if defined(CONFIG_ARCH_ELBRUS)
	return;
#elif defined(CONFIG_ARCH_MT6753) || defined(CONFIG_ARCH_MT6795)
	if (g_od_ddp_notify != NULL)
		g_od_ddp_notify(DISP_MODULE_OD, DISP_PATH_EVENT_TRIGGER);
#else

	if (g_od_ddp_notify != NULL)
		g_od_ddp_notify(DISP_MODULE_OD, DISP_PATH_EVENT_OD_TRIGGER);
#endif
}


void od_debug_reg(void)
{
	ODDBG(OD_LOG_ALWAYS, "==DISP OD REGS==");
	ODDBG(OD_LOG_ALWAYS, "OD:0x000=0x%08x,0x004=0x%08x,0x008=0x%08x,0x00c=0x%08x",
		OD_REG_GET(DISP_REG_OD_EN),
		OD_REG_GET(DISP_REG_OD_RESET),
		OD_REG_GET(DISP_REG_OD_INTEN),
		OD_REG_GET(DISP_REG_OD_INTSTA));

	ODDBG(OD_LOG_ALWAYS, "OD:0x010=0x%08x,0x020=0x%08x,0x024=0x%08x,0x028=0x%08x",
		OD_REG_GET(DISP_REG_OD_STATUS),
		OD_REG_GET(DISP_REG_OD_CFG),
		OD_REG_GET(DISP_REG_OD_INPUT_COUNT),
		OD_REG_GET(DISP_REG_OD_OUTPUT_COUNT));

	ODDBG(OD_LOG_ALWAYS, "OD:0x02c=0x%08x,0x030=0x%08x,0x040=0x%08x,0x044=0x%08x",
		OD_REG_GET(DISP_REG_OD_CHKSUM),
		OD_REG_GET(DISP_REG_OD_SIZE),
		OD_REG_GET(DISP_REG_OD_HSYNC_WIDTH),
		OD_REG_GET(DISP_REG_OD_VSYNC_WIDTH));

	ODDBG(OD_LOG_ALWAYS, "OD:0x048=0x%08x,0x0C0=0x%08x",
		OD_REG_GET(DISP_REG_OD_MISC),
		OD_REG_GET(DISP_REG_OD_DUMMY_REG));

	ODDBG(OD_LOG_ALWAYS, "OD:0x684=0x%08x,0x688=0x%08x,0x68c=0x%08x,0x690=0x%08x",
		OD_REG_GET(DISPSYS_OD_BASE + 0x684),
		OD_REG_GET(DISPSYS_OD_BASE + 0x688),
		OD_REG_GET(DISPSYS_OD_BASE + 0x68c),
		OD_REG_GET(DISPSYS_OD_BASE + 0x690));

	ODDBG(OD_LOG_ALWAYS, "OD:0x694=0x%08x,0x698=0x%08x,0x700=0x%08x,0x704=0x%08x",
		OD_REG_GET(DISPSYS_OD_BASE + 0x694),
		OD_REG_GET(DISPSYS_OD_BASE + 0x698),
		OD_REG_GET(DISPSYS_OD_BASE + 0x700),
		OD_REG_GET(DISPSYS_OD_BASE + 0x704));

	ODDBG(OD_LOG_ALWAYS, "OD:0x708=0x%08x,0x778=0x%08x,0x78c=0x%08x,0x790=0x%08x",
		OD_REG_GET(DISPSYS_OD_BASE + 0x708),
		OD_REG_GET(DISPSYS_OD_BASE + 0x778),
		OD_REG_GET(DISPSYS_OD_BASE + 0x78c),
		OD_REG_GET(DISPSYS_OD_BASE + 0x790));

	ODDBG(OD_LOG_ALWAYS, "OD:0x7a0=0x%08x,0x7dc=0x%08x,0x7e8=0x%08x",
		OD_REG_GET(DISPSYS_OD_BASE + 0x7a0),
		OD_REG_GET(DISPSYS_OD_BASE + 0x7dc),
		OD_REG_GET(DISPSYS_OD_BASE + 0x7e8));

}


/* NOTE: OD is not really enabled here until disp_od_start_read() is called */
void _od_core_set_enabled(void *cmdq, int enabled)
{
#if defined(CONFIG_MTK_OD_SUPPORT)
	/* od_debug_reg(); */

	/* dram and bypass setting */

	/* We will set ODT_MAX_RATIO to valid value to enable OD */
	OD_REG_SET_FIELD(cmdq, OD_REG37, 0x0, ODT_MAX_RATIO);

	OD_REG_SET_FIELD(cmdq, OD_REG02, 0, ODT_BYPASS);
	OD_REG_SET_FIELD(cmdq, OD_REG71, 1, RG_WDRAM_HOLD_EN);
	OD_REG_SET_FIELD(cmdq, OD_REG72, 1, RG_RDRAM_HOLD_EN);
	OD_REG_SET_FIELD(cmdq, OD_REG39, 0, WDRAM_DIS);
	OD_REG_SET_FIELD(cmdq, OD_REG39, 0, RDRAM_DIS);

	if (enabled == 1)
		DISP_REG_MASK(cmdq, DISP_REG_OD_CFG, 0x2, 0x3); /* core en */
	else
		DISP_REG_MASK(cmdq, DISP_REG_OD_CFG, 0x1, 0x3); /* Relay mode */
#endif
}


void disp_od_irq_handler(void)
{
#if defined(CONFIG_MTK_OD_SUPPORT)
	unsigned int od_istra = DISP_REG_GET(DISP_REG_OD_INTSTA);

	/* clear interrupt flag if "1" */
	DISP_REG_SET(NULL, DISP_REG_OD_INTSTA, (od_istra & ~0x1ff));

	/* detect certain interrupt for action */
	if ((od_istra & 0x2) != 0) {/* check OD output frame end interrupt */
		g_od_debug_cnt.frame++;
	}
#endif

	DISP_REG_SET(NULL, DISP_REG_OD_INTSTA, 0);
}


int disp_rdma_notify(unsigned int reg)
{
#if defined(CONFIG_MTK_OD_SUPPORT)
	if ((reg & (1 << 4)) != 0)
		g_od_debug_cnt.rdma_error++;
	else if ((reg & (1 << 1)) != 0)
		g_od_debug_cnt.rdma_normal++;
#endif

	return 0;
}


int disp_od_update_status(void *cmdq) /* Linked from primary_display.c */
{
	/* Do nothing */
	return 0;
}


static void _od_set_dram_buffer_addr(void *cmdq, int manual_comp, int image_width, int image_height)
{
	u32 u4ODDramSize;
	u32 u4Linesize;
	u32 od_buf_pa_32;

	static int is_inited;

	/* set line size : ( h active/4* manual CR )/128  ==>linesize = (h active * manual CR)/512*/
	u4Linesize = ((image_width * manual_comp) >> 9) + OD_LINESIZE_BUFFER;
	u4ODDramSize = u4Linesize * (image_height / 2) * 16;

	if (!is_inited) {
		void *va;
		dma_addr_t dma_addr;

		va = dma_alloc_coherent(disp_get_device(), u4ODDramSize + OD_ADDITIONAL_BUFFER + OD_GUARD_PATTERN_SIZE,
			&dma_addr, GFP_KERNEL);

		if (va == NULL) {
			ODDBG(OD_LOG_ALWAYS, "OD: MEM NOT ENOUGH %d", u4Linesize);
			BUG();
		}

		ODDBG(OD_LOG_ALWAYS, "OD: pa %08lx size %d order %d va %lx",
			(unsigned long)(dma_addr), u4ODDramSize, get_order(u4ODDramSize), (unsigned long)va);

		is_inited = 1;

		g_od_buf.size = u4ODDramSize;
		g_od_buf.pa = (unsigned long)dma_addr;
		g_od_buf.va = va;

		/* set guard pattern */
		*((u32 *)((unsigned long)va + u4ODDramSize)) = OD_GUARD_PATTERN;
		*((u32 *)((unsigned long)va + u4ODDramSize + OD_ADDITIONAL_BUFFER)) = OD_GUARD_PATTERN;
	}

	od_buf_pa_32 = (u32)g_od_buf.pa;

	OD_REG_SET_FIELD(cmdq, OD_REG06, (od_buf_pa_32 >> 4), RG_BASE_ADR);
	OD_REG_SET_FIELD(cmdq, OD_REG56, ((od_buf_pa_32 + u4ODDramSize) >> 4), DRAM_UPBOUND);
	OD_REG_SET_FIELD(cmdq, OD_REG56, 1, DRAM_PROT);
}


static void _od_set_frame_protect_init(void *cmdq, int image_width, int image_height)
{
	OD_REG_SET_FIELD(cmdq, OD_REG08, image_width, OD_H_ACTIVE);
	OD_REG_SET_FIELD(cmdq, OD_REG32, image_width, OD_DE_WIDTH);

	OD_REG_SET_FIELD(cmdq, OD_REG08, image_height, OD_V_ACTIVE);

	OD_REG_SET_FIELD(cmdq, OD_REG53, 0x0BFB, FRAME_ERR_CON);   /* don't care v blank */
	OD_REG_SET_FIELD(cmdq, OD_REG09, 0x01E, RG_H_BLANK);       /* h_blank  = htotal - h_active */
	OD_REG_SET_FIELD(cmdq, OD_REG09, 0x0A, RG_H_OFFSET);       /* tolerrance */

	OD_REG_SET_FIELD(cmdq, OD_REG10, 0xFFF, RG_H_BLANK_MAX);
	OD_REG_SET_FIELD(cmdq, OD_REG10, 0x3FFFF, RG_V_BLANK_MAX); /* pixel-based counter */

	OD_REG_SET_FIELD(cmdq, OD_REG11, 0xB000, RG_V_BLANK);      /* v_blank  = vtotal - v_active */
	OD_REG_SET_FIELD(cmdq, OD_REG11, 2, RG_FRAME_SET);
}


static void _od_set_param(void *cmdq, int manual_comp, int image_width, int image_height)
{
	u32 u4GMV_width;
	u32 u4Linesize;

	/* set gmv detection width */
	u4GMV_width = image_width / 6;

	OD_REG_SET_FIELD(cmdq, OD_REG40, (u4GMV_width*1)>>4, GM_R0_CENTER);
	OD_REG_SET_FIELD(cmdq, OD_REG40, (u4GMV_width*2)>>4, GM_R1_CENTER);
	OD_REG_SET_FIELD(cmdq, OD_REG41, (u4GMV_width*3)>>4, GM_R2_CENTER);
	OD_REG_SET_FIELD(cmdq, OD_REG41, (u4GMV_width*4)>>4, GM_R3_CENTER);
	OD_REG_SET_FIELD(cmdq, OD_REG42, (u4GMV_width*5)>>4, GM_R4_CENTER);

	OD_REG_SET_FIELD(cmdq, OD_REG43, 12 >> 2                 , GM_V_ST);
	OD_REG_SET_FIELD(cmdq, OD_REG43, (image_height-12)>>2    , GM_V_END);
	OD_REG_SET_FIELD(cmdq, OD_REG42, (100*image_height)/1080 , GM_LGMIN_DIFF);
	OD_REG_SET_FIELD(cmdq, OD_REG44, (400*image_height)/1080 , GM_LMIN_THR);
	OD_REG_SET_FIELD(cmdq, OD_REG44, (200*image_height)/1080 , GM_GMIN_THR);

	/* set compression ratio */
	OD_REG_SET_FIELD(cmdq, OD_REG30, manual_comp, MANU_CPR);

	/* set line size */
	/* linesize = ( h active/4* manual CR )/128  ==>linesize = (h active * manual CR)/512 */
	u4Linesize = ((image_width * manual_comp) >> 9) + 2;
	OD_REG_SET_FIELD(cmdq, OD_REG47, 3, PRE_BW);  /* vIO32WriteFldAlign(OD_REG47, 3, PRE_BW); */

	OD_REG_SET_FIELD(cmdq, OD_REG34, 0xF,  ODT_SB_TH0);
	OD_REG_SET_FIELD(cmdq, OD_REG34, 0x10, ODT_SB_TH1);
	OD_REG_SET_FIELD(cmdq, OD_REG34, 0x11, ODT_SB_TH2);
	OD_REG_SET_FIELD(cmdq, OD_REG34, 0x12, ODT_SB_TH3);

	OD_REG_SET_FIELD(cmdq, OD_REG47, 0x13, ODT_SB_TH4);
	OD_REG_SET_FIELD(cmdq, OD_REG47, 0x14, ODT_SB_TH5);
	OD_REG_SET_FIELD(cmdq, OD_REG47, 0x15, ODT_SB_TH6);
	OD_REG_SET_FIELD(cmdq, OD_REG47, 0x16, ODT_SB_TH7);

	OD_REG_SET_FIELD(cmdq, OD_REG38, u4Linesize, LINE_SIZE);

	/* use 64 burst length */
	OD_REG_SET_FIELD(cmdq, OD_REG38, 3, WR_BURST_LEN);
	OD_REG_SET_FIELD(cmdq, OD_REG38, 3, RD_BURST_LEN);

	/*set auto 8bit parameters */
	if (image_width > 1900) {
		OD_REG_SET_FIELD(cmdq, OD_REG35, (140000 << 0), DET8B_DC_NUM);
		OD_REG_SET_FIELD(cmdq, OD_REG36, (40000 << 18), DET8B_BTC_NUM);
		OD_REG_SET_FIELD(cmdq, OD_REG37, (1900000>>4) , DET8B_BIT_MGN);
	} else {
		OD_REG_SET_FIELD(cmdq, OD_REG35, 70000, DET8B_DC_NUM);
		OD_REG_SET_FIELD(cmdq, OD_REG36, 20000, DET8B_BTC_NUM);
		OD_REG_SET_FIELD(cmdq, OD_REG37, (950000>>4), DET8B_BIT_MGN);
	}

	/* set dump param */
	OD_REG_SET_FIELD(cmdq, OD_REG51, 0, DUMP_STLINE);
	OD_REG_SET_FIELD(cmdq, OD_REG51, (image_height-1), DUMP_ENDLINE);
}


static void _od_write_table(void *cmdq, u8 TableSel, u8 ColorSel, const u8 *pTable, int table_inverse)
{
	u32 i, u4TblSize;
	u32 u1ODBypass   = DISP_REG_GET(OD_REG02) & (1 << 9)  ? 1 : 0;
	u32 u1FBBypass   = DISP_REG_GET(OD_REG02) & (1 << 10) ? 1 : 0;
	u32 u1PCIDBypass = DISP_REG_GET(OD_REG02) & (1 << 21) ? 1 : 0;

	if (ColorSel > 3)
		return;

	/* disable OD_START */
	DISP_REG_SET(cmdq, OD_REG12, 0);

	OD_REG_SET_FIELD(cmdq, OD_REG02, 1, ODT_BYPASS);
	OD_REG_SET_FIELD(cmdq, OD_REG02, 1, FBT_BYPASS);

	OD_REG_SET_FIELD(cmdq, OD_REG45, 0, OD_PCID_EN);
	OD_REG_SET_FIELD(cmdq, OD_REG45, 1, OD_PCID_BYPASS);
	OD_REG_SET_FIELD(cmdq, OD_REG04, 1, TABLE_ONLY_W_ADR_INC);
	OD_REG_SET_FIELD(cmdq, OD_REG04, 0, ADDR_YX);
	OD_REG_SET_FIELD(cmdq, OD_REG04, 0x3, TABLE_RW_SEL_OD_BGR);

	if (ColorSel == 3)
		OD_REG_SET_FIELD(cmdq, OD_REG04, 7, TABLE_RW_SEL_OD_BGR);

	else
		OD_REG_SET_FIELD(cmdq, OD_REG04, (1 << ColorSel), TABLE_RW_SEL_OD_BGR);

	switch (TableSel) {
	case OD_TABLE_33:
		u4TblSize = OD_TBL_M_SIZE;
		OD_REG_SET_FIELD(cmdq, OD_REG32, 0, OD_IDX_41);
		OD_REG_SET_FIELD(cmdq, OD_REG32, 0, OD_IDX_17);
		break;

	case OD_TABLE_17:
		u4TblSize = OD_TBL_S_SIZE;
		OD_REG_SET_FIELD(cmdq, OD_REG32, 0, OD_IDX_41);
		OD_REG_SET_FIELD(cmdq, OD_REG32, 1, OD_IDX_17);
		break;

	default:
		return;
	}

	for (i = 0; i < u4TblSize; i++) {
		if (table_inverse) {
			u8 value = ABS(255 - *(pTable+i));

			DISP_REG_SET(cmdq, OD_REG05, value);
		} else {
			DISP_REG_SET(cmdq, OD_REG05, *(pTable+i));
		}
	}

	DISP_REG_SET(cmdq, OD_REG04, 0);
	OD_REG_SET_FIELD(cmdq, OD_REG02,  u1ODBypass, ODT_BYPASS);
	OD_REG_SET_FIELD(cmdq, OD_REG02,  u1FBBypass, FBT_BYPASS);
	OD_REG_SET_FIELD(cmdq, OD_REG45,  (!u1PCIDBypass), OD_PCID_EN);
	OD_REG_SET_FIELD(cmdq, OD_REG45,  (u1PCIDBypass), OD_PCID_BYPASS);
}


static u8 _od_read_table(void *cmdq, u8 TableSel, u8 ColorSel, const u8 *pTable, int table_inverse)
{
	u32 i, u4TblVal, u4TblSize, u4ErrCnt = 0;
	u32 mask;
	u32 u1ODBypass    = DISP_REG_GET(OD_REG02) & (1 << 9)  ? 1 : 0;
	u32 u1FBBypass    = DISP_REG_GET(OD_REG02) & (1 << 10) ? 1 : 0;
	u32 u1PCIDBypass  = DISP_REG_GET(OD_REG02) & (1 << 21) ? 1 : 0;

	if (ColorSel > 2)
		return 1;

	OD_REG_SET_FIELD(cmdq, OD_REG02, u1ODBypass, ODT_BYPASS);
	OD_REG_SET_FIELD(cmdq, OD_REG02, u1FBBypass, FBT_BYPASS);

	OD_REG_SET_FIELD(cmdq, OD_REG04, 0, TABLE_ONLY_W_ADR_INC);
	OD_REG_SET_FIELD(cmdq, OD_REG04, 0, ADDR_YX);

	mask = ~(0x7 << 19);
	OD_REG_SET_FIELD(cmdq, OD_REG04, 7, TABLE_RW_SEL_OD_BGR);

	switch (TableSel) {
	case OD_TABLE_33:
		u4TblSize = OD_TBL_M_SIZE;
		OD_REG_SET_FIELD(cmdq, OD_REG32, 0, OD_IDX_41);
		OD_REG_SET_FIELD(cmdq, OD_REG32, 0, OD_IDX_17);
		break;

	case OD_TABLE_17:
		u4TblSize = OD_TBL_S_SIZE;
		OD_REG_SET_FIELD(cmdq, OD_REG32, 0, OD_IDX_41);
		OD_REG_SET_FIELD(cmdq, OD_REG32, 1, OD_IDX_17);
		break;

	default:
		return 0;
	}

	for (i = 0; i < u4TblSize; i++) {
		u4TblVal = DISP_REG_GET(OD_REG05);

		if (table_inverse) {
			u8 value = ABS(255 - *(pTable+i));

			if (value != u4TblVal)
				u4ErrCnt++;
		} else {
			if (*(pTable+i) != u4TblVal) {
				ODDBG(OD_LOG_ALWAYS, "OD %d TBL %d %d != %d", ColorSel, i, *(pTable+i), u4TblVal);
				u4ErrCnt++;
			}
		}
	}

	DISP_REG_SET(cmdq, OD_REG04, 0);
	OD_REG_SET_FIELD(cmdq, OD_REG02, u1ODBypass, ODT_BYPASS);
	OD_REG_SET_FIELD(cmdq, OD_REG02, u1FBBypass, FBT_BYPASS);
	OD_REG_SET_FIELD(cmdq, OD_REG45, !u1PCIDBypass, OD_PCID_EN);
	OD_REG_SET_FIELD(cmdq, OD_REG45, u1PCIDBypass, OD_PCID_BYPASS);

	return u4ErrCnt;
}


static void _od_set_table(void *cmdq, int tableSelect, const u8 *od_table, int table_inverse)
{
	DISP_REG_SET(cmdq, DISP_REG_OD_MISC, 1); /* [1]:can access OD table; [0]:can't access OD table */

	/* Write OD table */
	if (OD_TABLE_17 == tableSelect) {
		_od_write_table(cmdq, OD_TABLE_17, OD_ALL, od_table, table_inverse);

		/* Check OD table */
		/*
		#ifndef DEF_CMDQ
		_od_read_table(OD_TABLE_17, OD_RED, OD_Table_17x17, table_inverse);
		_od_read_table(OD_TABLE_17, OD_GREEN, OD_Table_17x17, table_inverse);
		_od_read_table(OD_TABLE_17, OD_BLUE, OD_Table_17x17, table_inverse);
		#endif
		*/

		DISP_REG_SET(cmdq, OD_REG02, (1 << 10));
		OD_REG_SET_FIELD(cmdq, OD_REG45, 1, OD_PCID_BYPASS);
		OD_REG_SET_FIELD(cmdq, OD_REG45, 0, OD_PCID_EN);
		DISP_REG_SET(cmdq, OD_REG12, 1 << 0);
	} else if (OD_TABLE_33 == tableSelect) {
		_od_write_table(cmdq, OD_TABLE_33, OD_ALL, od_table, table_inverse);

		/* Check OD table */
		/*
		#ifndef DEF_CMDQ
		 _od_read_table(OD_TABLE_33, OD_RED, OD_Table_33x33, table_inverse);
		 _od_read_table(OD_TABLE_33, OD_GREEN, OD_Table_33x33, table_inverse);
		_od_read_table(OD_TABLE_33, OD_BLUE, OD_Table_33x33, table_inverse);
		#endif
		*/

		DISP_REG_SET(cmdq, OD_REG02, (1 << 10));
		OD_REG_SET_FIELD(cmdq, OD_REG45, 1, OD_PCID_BYPASS);
		OD_REG_SET_FIELD(cmdq, OD_REG45, 0, OD_PCID_EN);
		DISP_REG_SET(cmdq, OD_REG12, 1 << 0);
	} else {
		ODDBG(OD_LOG_ALWAYS, "Error OD table");
		BUG();
	}

	DISP_REG_SET(cmdq, DISP_REG_OD_MISC, 0); /* [1]:can access OD table; [0]:can't access OD table */
}


void disp_config_od(unsigned int width, unsigned int height, void *cmdq, unsigned int od_table_size, void *od_table)
{
	int manual_cpr = OD_MANUAL_CPR;

	int od_table_select = 0;

	ODDBG(OD_LOG_VERBOSE, "OD conf start %lx %x %lx", (unsigned long)cmdq, od_table_size, (unsigned long)od_table);

	switch (od_table_size) {
	case 17*17:
		od_table_select = OD_TABLE_17;
		break;

	case 33*33:
		od_table_select = OD_TABLE_33;
		break;

	/* default linear table */
	default:
		od_table_select = OD_TABLE_17;
		od_table = (void *)OD_Table_dummy_17x17;
		break;
	}

	if (od_table == NULL) {
		ODDBG(OD_LOG_ALWAYS, "LCM NULL OD table");
		BUG();
	}

	_od_set_table(cmdq, od_table_select, od_table, 0);
	_od_set_dram_buffer_addr(cmdq, manual_cpr, width, height);
	_od_set_frame_protect_init(cmdq, width, height);
	_od_set_param(cmdq, manual_cpr, width, height);

	DISP_REG_SET(cmdq, DISP_REG_OD_EN, 1);

	DISP_REG_SET(cmdq, DISP_REG_OD_SIZE, (width << 16) | height);
	DISP_REG_SET(cmdq, DISP_REG_OD_HSYNC_WIDTH, OD_HSYNC_WIDTH);
	DISP_REG_SET(cmdq, DISP_REG_OD_VSYNC_WIDTH, (OD_HSYNC_WIDTH << 16) | (width * 3 / 2));

	ODDBG(OD_LOG_VERBOSE, "OD inited W %d H %d", width, height);
}


int disp_od_get_enabled(void)
{
	return g_od_is_enabled;
}


void disp_od_mhl_force(int allow_enabled)
{
	ODDBG(OD_LOG_ALWAYS, "disp_od_mhl_force(allow = %d)", allow_enabled);

	if (!allow_enabled)
		set_bit(DISABLED_BY_MHL, &g_od_force_disabled);
	else
		clear_bit(DISABLED_BY_MHL, &g_od_force_disabled);
}


void disp_od_hwc_force(int allow_enabled)
{
	ODDBG(OD_LOG_ALWAYS, "disp_od_hwc_force(allow = %d)", allow_enabled);

	if (!allow_enabled)
		set_bit(DISABLED_BY_HWC, &g_od_force_disabled);
	else
		clear_bit(DISABLED_BY_HWC, &g_od_force_disabled);
}


void disp_od_set_smi_clock(int enabled)
{
#ifndef CONFIG_MTK_CLKMGR
	eDDP_CLK_ID larb_clk;

	ODDBG(OD_LOG_ALWAYS, "disp_od_set_smi_clock(%d), od_enabled=%d", enabled, g_od_is_enabled);

#if defined(CONFIG_ARCH_MT6757) || defined(CONFIG_ARCH_ELBRUS)
	larb_clk = DISP0_SMI_LARB4;
#else
	larb_clk = DISP0_SMI_LARB5;
#endif

	if (enabled) {
		ddp_clk_prepare_enable(DISP0_SMI_COMMON);
		ddp_clk_prepare_enable(larb_clk);
	} else {
		ddp_clk_disable_unprepare(larb_clk);
		ddp_clk_disable_unprepare(DISP0_SMI_COMMON);
	}
#else
	ODDBG(OD_LOG_ALWAYS, "disp_od_set_smi_clock not support");
#endif
}

void disp_od_set_enabled(void *cmdq, int enabled)
{
#if defined(CONFIG_MTK_OD_SUPPORT)
	int to_enable = 0;

	mutex_lock(&g_od_global_lock);
	if (g_od_debug_enable == DEBUG_ENABLE_NORMAL) {
		if (!enabled || g_od_force_disabled != 0)
			to_enable = 0;
		else
			to_enable = 1;
	} else if (g_od_debug_enable == DEBUG_ENABLE_ALWAYS)
		to_enable = 1;
	else
		to_enable = 0;

	if (to_enable != g_od_is_enabled) {
		g_od_is_enabled = to_enable;
		disp_od_set_smi_clock(g_od_is_enabled);
	}

	_od_core_set_enabled(cmdq, g_od_is_enabled);
	mutex_unlock(&g_od_global_lock);

	if (!to_enable)
		od_refresh_screen();

	ODDBG(OD_LOG_ALWAYS, "disp_od_set_enabled(in:%d) result=%d, (force_disabled:0x%lx)",
		enabled, to_enable, g_od_force_disabled);
#endif
}


/*
 * Must be called after the 3rd frame after disp_od_set_enabled(1)
 * to enable OD function
 */
void disp_od_start_read(void *cmdq)
{
#if defined(CONFIG_MTK_OD_SUPPORT)
	mutex_lock(&g_od_global_lock);
	OD_REG_SET_FIELD(cmdq, OD_REG37, 0xf, ODT_MAX_RATIO);
	ODDBG(OD_LOG_ALWAYS, "disp_od_start_read(): enabled = %d", g_od_is_enabled);
	mutex_unlock(&g_od_global_lock);
#endif
}


static int disp_od_ioctl_ctlcmd(DISP_MODULE_ENUM module, int msg, unsigned long arg, void *cmdq)
{
	DISP_OD_CMD cmd;

	if (copy_from_user((void *)&cmd, (void *)arg, sizeof(DISP_OD_CMD)))
		return -EFAULT;

	ODDBG(OD_LOG_ALWAYS, "OD ioctl cmdq %lx", (unsigned long)cmdq);

	switch (cmd.type) {
	case OD_CTL_ENABLE: /* on/off OD */
		if (cmd.param0 == OD_CTL_ENABLE_OFF) {
			disp_od_hwc_force(0);
			disp_od_set_enabled(cmdq, 0);
		} else if (cmd.param0 == OD_CTL_ENABLE_ON) {
			disp_od_hwc_force(1);
			disp_od_set_enabled(cmdq, 1);
		} else {
			ODDBG(OD_LOG_ALWAYS, "unknown enable type command");
		}
		break;

	case OD_CTL_READ_REG: /* read reg */
		if (cmd.param0 < 0x1000) { /* deny OOB access */
			cmd.ret = OD_REG_GET(cmd.param0 + OD_BASE);
		} else {
			cmd.ret = 0;
		}
		break;

	case OD_CTL_WRITE_REG: /* write reg */
		if (cmd.param0 < 0x1000) { /* deny OOB access */
			DISP_REG_SET(cmdq, (unsigned long)(cmd.param0 + OD_BASE), cmd.param1);
			cmd.ret = OD_REG_GET(cmd.param0 + OD_BASE);
		} else {
			cmd.ret = 0;
		}
		break;

	/* enable split screen OD demo mode for miravision */
	case OD_CTL_ENABLE_DEMO_MODE:
	{
		switch (cmd.param0) {
		/* demo mode */
		case 0:
		case 1:
		{
			int enable = cmd.param0 ? 1 : 0;

			OD_REG_SET_FIELD(cmdq, OD_REG02, enable, DEMO_MODE);
			ODDBG(OD_LOG_ALWAYS, "OD demo %d", enable);
			/* save demo mode flag for suspend/resume */
			g_od_is_demo_mode = enable;
			break;
		}

		/* enable ink */
		case 2: /* off */
		case 3: /* on */
			OD_REG_SET_FIELD(cmdq, OD_REG03, (cmd.param0 - 2), ODT_INK_EN);
			break;

		/* eanble debug OSD */
		case 4: /* off */
		case 5: /* on */
			OD_REG_SET_FIELD(cmdq, OD_REG46, (cmd.param0 - 4), OD_OSD_SEL);
			break;

		default:
			break;
		}

	}
	break;

	/* write od table */
	case OD_CTL_WRITE_TABLE:
		return -EFAULT;

	default:
		break;
	}

	if (copy_to_user((void *)arg, (void *)&cmd, sizeof(DISP_OD_CMD)))
		return -EFAULT;

	return 0;
}

#if defined(CONFIG_ARCH_MT6797)
static int _od_partial_update(DISP_MODULE_ENUM module, void *arg, void *cmdq)
{
	struct disp_rect *roi = (struct disp_rect *) arg;
	int width = roi->width;
	int height = roi->height;

	DISP_REG_SET(cmdq, DISP_REG_OD_SIZE, (width << 16) | height);
	return 0;
}

static int disp_od_io(DISP_MODULE_ENUM module, void *handle,
		DDP_IOCTL_NAME ioctl_cmd, void *params)
{
	int ret = -1;

	if (ioctl_cmd == DDP_PARTIAL_UPDATE) {
		_od_partial_update(module, params, handle);
		ret = 0;
	}
	return ret;
}
#endif

static int disp_od_ioctl(DISP_MODULE_ENUM module, int msg, unsigned long arg, void *cmdq)
{
	switch (msg) {
	case DISP_IOCTL_OD_CTL:
		return disp_od_ioctl_ctlcmd(module, msg, arg, cmdq);

	default:
		return -EFAULT;
	}

	return 0;
}


static void ddp_bypass_od(unsigned int width, unsigned int height, void *handle)
{
	ODNOTICE("ddp_bypass_od");
	DISP_REG_SET(handle, DISP_REG_OD_SIZE, (width << 16) | height);
	DISP_REG_SET(handle, DISP_REG_OD_CFG, 0x1);
	DISP_REG_SET(handle, DISP_REG_OD_EN, 0x1);
}


static int od_config_od(DISP_MODULE_ENUM module, disp_ddp_path_config *pConfig, void *cmdq)
{
#if defined(CONFIG_MTK_OD_SUPPORT)
	const LCM_PARAMS *lcm_param = &(pConfig->dispif_config);
#endif

	if (pConfig->dst_dirty) {
#if defined(CONFIG_MTK_OD_SUPPORT)
		unsigned int od_table_size = lcm_param->od_table_size;
		void *od_table = lcm_param->od_table;

		if (od_table != NULL)
			ODDBG(OD_LOG_ALWAYS, "od_config_od: LCD OD table size = %u", od_table_size);

	#if defined(OD_ALLOW_DEFAULT_TABLE)
		if (od_table == NULL) {
			od_table_size = 33 * 33;
			od_table = (void *)OD_Table_33x33;
			ODDBG(OD_LOG_ALWAYS, "od_config_od: Use default 33x33 table");
		}
	#endif
	#if defined(OD_LINEAR_TABLE_IF_NONE)
		if (od_table == NULL) {
			od_table_size = 17 * 17;
			od_table = (void *)OD_Table_dummy_17x17;
			ODDBG(OD_LOG_ALWAYS, "od_config_od: Use dummy 17x17 table");
		}
	#endif

		if (od_table != NULL) {
			/* spm_enable_sodi(0); */
			od_start(module, cmdq);
			disp_config_od(pConfig->dst_w, pConfig->dst_h, cmdq, od_table_size, od_table);
	#if 0
			/* For debug */
			DISP_REG_MASK(cmdq, DISP_REG_OD_INTEN, (1<<6)|(1<<3)|(1<<2), (1<<6)|(1<<3)|(1<<2));
	#endif
		} else {
			ddp_bypass_od(pConfig->dst_w, pConfig->dst_h, cmdq);
			ODDBG(OD_LOG_ALWAYS, "od_config_od: No od table bypass");
		}

#else /* Not support OD */
		ddp_bypass_od(pConfig->dst_w, pConfig->dst_h, cmdq);
		ODDBG(OD_LOG_ALWAYS, "od_config_od: Not support od bypass");
#endif
	}

#if defined(CONFIG_MTK_OD_SUPPORT)
	if (!pConfig->dst_dirty &&
		(lcm_param->type == LCM_TYPE_DSI) && (lcm_param->dsi.mode == CMD_MODE)) {
		if (pConfig->ovl_dirty || pConfig->rdma_dirty)
			od_refresh_screen();
	}
#endif

	return 0;
}


static void od_set_debug_function(void *cmdq)
{
	if (g_od_debug_mode != 0) {
		if (g_od_debug_mode & DEBUG_MODE_OSD)
			OD_REG_SET_FIELD(cmdq, OD_REG46, 1, OD_OSD_SEL);
		else
			OD_REG_SET_FIELD(cmdq, OD_REG46, 0, OD_OSD_SEL);

		if (g_od_debug_mode & DEBUG_MODE_INK)
			DISP_REG_MASK(cmdq, OD_REG03, (0x33 << 24) | (0xaa << 16) | (0x55 << 8) | 1, 0xffffff01);
		else
			DISP_REG_MASK(cmdq, OD_REG03, 0, 0x1);
	} else {
		OD_REG_SET_FIELD(cmdq, OD_REG46, 0, OD_OSD_SEL);
		DISP_REG_MASK(cmdq, OD_REG03, 0, 0x1);
	}
}


#if defined(CONFIG_MTK_OD_SUPPORT)
static int od_start(DISP_MODULE_ENUM module, void *cmdq)
{
	ODDBG(OD_LOG_ALWAYS, "od_start()");

	_od_reg_init(cmdq);

	/* OD on/off align to vsync */
	DISP_REG_SET(cmdq, OD_REG53, 0x6BFB7E00);

	/* modified ALBUF2_DLY OD_REG01 */

	/* OD_REG_SET_FIELD(cmdq, OD_REG01, (0xD), ALBUF2_DLY); // 1080w */
	OD_REG_SET_FIELD(cmdq, OD_REG01, (0xE), ALBUF2_DLY); /* 720w */


	/* disable hold for debug */
	/*
	OD_REG_SET_FIELD(cmdq, OD_REG71, 0, RG_WDRAM_HOLD_EN);
	OD_REG_SET_FIELD(cmdq, OD_REG72, 0, RG_RDRAM_HOLD_EN);
	*/

	/* enable debug OSD for status reg */
	/* OD_REG_SET_FIELD(cmdq, OD_REG46, 1, OD_OSD_SEL); */

	/* lower hold threshold */
	/*
	OD_REG_SET_FIELD(cmdq, OD_REG73, 0, RG_WDRAM_HOLD_THR);
	OD_REG_SET_FIELD(cmdq, OD_REG73, 0, RG_RDRAM_HOLD_THR);
	*/

	/* restore demo mode for suspend / resume */
	if (g_od_is_demo_mode)
		OD_REG_SET_FIELD(cmdq, OD_REG02, 1, DEMO_MODE);

	OD_REG_SET_FIELD(cmdq, OD_REG00, 0, BYPASS_ALL);

	/* clear crc error first */
	OD_REG_SET_FIELD(cmdq, OD_REG38, 1, DRAM_CRC_CLR);

	DISP_REG_MASK(cmdq, DISP_REG_OD_INTEN, 0x43, 0xff);

#if 0
	/*
	 * Set WDMA/RDMA burst to 0x80.
	 * Due to EMI limitation, burst x size must < 128; otherwise, EMI will hang.
	 */
	DISP_REG_MASK(cmdq, OD_BASE + 0x208, (0x8 << 20), (0x3f << 20)); /* WDMA */
	DISP_REG_MASK(cmdq, OD_BASE + 0x108, (0x8 << 20), (0x3f << 20)); /* RDMA */
#endif

	/* set auto Y5 mode thr */
	OD_REG_SET_FIELD(cmdq, OD_REG46,  0x4E00, AUTO_Y5_NUM);
	OD_REG_SET_FIELD(cmdq, OD_REG53,  0x4E00, AUTO_Y5_NUM_1);

	/* set OD threshold */
	OD_REG_SET_FIELD(cmdq, OD_REG01, 10, MOTION_THR);
	OD_REG_SET_FIELD(cmdq, OD_REG48, 8, ODT_INDIFF_TH);
	OD_REG_SET_FIELD(cmdq, OD_REG02, 1, FBT_BYPASS);

	/* set compression param */
	OD_REG_SET_FIELD(cmdq, OD_REG77, 0xfc, RC_U_RATIO);
	OD_REG_SET_FIELD(cmdq, OD_REG78, 0xfc, RC_U_RATIO_FIRST2);
	OD_REG_SET_FIELD(cmdq, OD_REG77, 0x68, RC_L_RATIO);

	OD_REG_SET_FIELD(cmdq, OD_REG78, 0x68, RC_L_RATIO_FIRST2);
	OD_REG_SET_FIELD(cmdq, OD_REG76, 0x3, CHG_Q_FREQ);

	OD_REG_SET_FIELD(cmdq, OD_REG76, 0x2, CURR_Q_UV);
	OD_REG_SET_FIELD(cmdq, OD_REG76, 0x2, CURR_Q_BYPASS);
	OD_REG_SET_FIELD(cmdq, OD_REG77, 0x8, IP_SAD_TH);

	OD_REG_SET_FIELD(cmdq, OD_REG71, 40, RG_WR_HIGH);
	OD_REG_SET_FIELD(cmdq, OD_REG71, 40, RG_WR_PRE_HIGH);
	OD_REG_SET_FIELD(cmdq, OD_REG71, 1, RG_WRULTRA_EN);
	OD_REG_SET_FIELD(cmdq, OD_REG71, 40, RG_WR_LOW);
	OD_REG_SET_FIELD(cmdq, OD_REG71, 40, RG_WR_PRELOW);
	OD_REG_SET_FIELD(cmdq, OD_REG71, 1, RG_WGPREULTRA_EN);
	OD_REG_SET_FIELD(cmdq, OD_REG71, 1, RG_WDRAM_HOLD_EN);
	OD_REG_SET_FIELD(cmdq, OD_REG71, 1, RG_WDRAM_LEN_X8);

	OD_REG_SET_FIELD(cmdq, OD_REG72, 40, RG_RD_HIGH);
	OD_REG_SET_FIELD(cmdq, OD_REG72, 40, RG_RD_PRE_HIGH);
	OD_REG_SET_FIELD(cmdq, OD_REG72, 1, RG_RDULTRA_EN);
	OD_REG_SET_FIELD(cmdq, OD_REG72, 40, RG_RD_LOW);
	OD_REG_SET_FIELD(cmdq, OD_REG72, 40, RG_RD_PRELOW);
	OD_REG_SET_FIELD(cmdq, OD_REG72, 1, RG_RGPREULTRA_EN);
	OD_REG_SET_FIELD(cmdq, OD_REG72, 1, RG_RDRAM_HOLD_EN);
	OD_REG_SET_FIELD(cmdq, OD_REG72, 1, RG_RDRAM_LEN_X8);

	/* DMA settings */
	DISP_REG_SET(cmdq, OD_BASE + 0x100, (620 << 20) | (65 << 10) | 440);
	DISP_REG_SET(cmdq, OD_BASE + 0x108, (16 << 26) | (8 << 20) | (600 << 10) | 10);
	DISP_REG_SET(cmdq, OD_BASE + 0x10c, (60 << 20) | (32 << 10) | 16);
	DISP_REG_SET(cmdq, OD_BASE + 0x200, (620 << 20) | (60 << 10) | 440);
	DISP_REG_SET(cmdq, OD_BASE + 0x208, (16 << 26) | (8 << 20) | (600 << 10) | 10);
	DISP_REG_SET(cmdq, OD_BASE + 0x20c, (450 << 20) | (500 << 10) | 540);

	od_set_debug_function(cmdq);

	mutex_lock(&g_od_global_lock);
	_od_core_set_enabled(cmdq, g_od_is_enabled);
	mutex_unlock(&g_od_global_lock);

	return 0;
}
#endif /* defined(CONFIG_MTK_OD_SUPPORT) */


static int od_clock_on(DISP_MODULE_ENUM module, void *handle)
{
	M4U_PORT_STRUCT m4u_port;

#ifdef ENABLE_CLK_MGR
#ifdef CONFIG_MTK_CLKMGR
	enable_clock(MT_CG_DISP0_DISP_OD, "od");
	DISPMSG("od_clock on CG 0x%x\n", DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
#else
	ddp_clk_enable(DISP0_DISP_OD);
#endif /* CONFIG_MTK_CLKMGR */
#endif /* ENABLE_CLK_MGR */

#ifdef CONFIG_MTK_OD_SUPPORT
	mutex_lock(&g_od_global_lock);
	if (g_od_is_enabled)
		disp_od_set_smi_clock(1);
	mutex_unlock(&g_od_global_lock);
#endif /* CONFIG_MTK_OD_SUPPORT */

	m4u_port.ePortID = M4U_PORT_DISP_OD_R;
	m4u_port.Virtuality = 0;
	m4u_port.Security = 0;
	m4u_port.domain = 0;
	m4u_port.Distance = 1;
	m4u_port.Direction = 0;
	m4u_config_port(&m4u_port);
	m4u_port.ePortID = M4U_PORT_DISP_OD_W;
	m4u_config_port(&m4u_port);

	return 0;
}


static int od_clock_off(DISP_MODULE_ENUM module, void *handle)
{
#ifdef ENABLE_CLK_MGR
#ifdef CONFIG_MTK_CLKMGR
	disable_clock(MT_CG_DISP0_DISP_OD , "od");
	DISPMSG("od_clock off CG 0x%x\n", DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
#else
	ddp_clk_disable(DISP0_DISP_OD);
#endif /* CONFIG_MTK_CLKMGR */
#endif /* ENABLE_CLK_MGR */

#ifdef CONFIG_MTK_OD_SUPPORT
	mutex_lock(&g_od_global_lock);
	if (g_od_is_enabled)
		disp_od_set_smi_clock(0);
	mutex_unlock(&g_od_global_lock);
#endif /* CONFIG_MTK_OD_SUPPORT */

	return 0;
}


/* for SODI to check OD is enabled or not, this will be called when screen is on and disp clock is enabled */
int disp_od_is_enabled(void)
{
	return (DISP_REG_GET(DISP_REG_OD_CFG) & (1 << 1)) ? 1 : 0;
}


static int od_set_listener(DISP_MODULE_ENUM module, ddp_module_notify notify)
{
	g_od_ddp_notify = notify;
	return 0;
}


/* OD driver module */
DDP_MODULE_DRIVER ddp_driver_od = {
	.init            = od_clock_on,
	.deinit          = od_clock_off,
	.config          = od_config_od,
	.trigger         = NULL,
	.stop            = NULL,
	.reset           = NULL,
	.power_on        = od_clock_on,
	.power_off       = od_clock_off,
	.is_idle         = NULL,
	.is_busy         = NULL,
	.dump_info       = NULL,
	.bypass          = NULL,
	.build_cmdq      = NULL,
	.set_lcm_utils   = NULL,
	.cmd             = disp_od_ioctl,
	.set_listener    = od_set_listener,
#if defined(CONFIG_ARCH_MT6797)
	.ioctl           = disp_od_io
#endif
};



/* ----------------------------------------------------------------------
// Test code
// Following is only for OD functional test, not normal code
// Will not be linked into user build.
// --------------------------------------------------------------------*/

#define OD_TLOG(fmt, arg...) pr_warn("[OD] " fmt "\n", ##arg)

/*
 * A replacement of obsolete simple_strtoul(). The user must be restricted
 * and the usage must be reviewed carefully to avoid potential security issue.
 */
static unsigned long od_simple_strtoul(char *next, char **new_next, int base)
{
	char buffer[20];
	int i;
	unsigned long value;

	for (i = 0; i < sizeof(buffer) - 1; i++) {
		char ch = next[i];

		if ((ch == 'x') || ('0' <= ch && ch <= '9') ||
			('a' <= ch && ch <= 'f') || ('A' <= ch && ch <= 'F')) {
			buffer[i] = ch;
		} else {
			buffer[i] = '\0';
			break;
		}
	}

	buffer[sizeof(buffer) - 1] = '\0';
	*new_next = &(next[i]);

	if (kstrtoul(buffer, base, &value) == 0)
		return value;

	return 0;
}

static int od_parse_triple(const char *cmd, unsigned long *offset, unsigned int *value, unsigned int *mask)
{
	int count = 0;
	char *next = (char *)cmd;

	*value = 0;
	*mask = 0;
	*offset = (unsigned long)od_simple_strtoul(next, &next, 0);

	if (*offset > 0x1000UL || (*offset & 0x3UL) != 0)  {
		*offset = 0UL;
		return 0;
	}
	count++;

	if (*next != ',')
		return count;
	next++;

	*value = (unsigned int)od_simple_strtoul(next, &next, 0);
	count++;

	if (*next != ',')
		return count;
	next++;

	*mask = (unsigned int)od_simple_strtoul(next, &next, 0);
	count++;

	return count;
}


static void od_dump_reg(const char *reg_list)
{
	unsigned long offset;
	unsigned int value;
	char *next = (char *)reg_list;

	OD_TLOG("OD reg base = %lx", (unsigned long)(OD_BASE));
	while (1) {
		offset = (unsigned long)od_simple_strtoul(next, &next, 0);
		if (offset < 0x1000UL && (offset & 0x3UL) == 0) {
			value = DISP_REG_GET(OD_BASE + offset);
			OD_TLOG("[+0x%03lx] = 0x%08x(%d)", offset, value, value);
		}

		if (next[0] != ',')
			break;

		next++;
	}
}


static void od_test_slow_mode(void)
{
#if 0
	msleep(30);

	/* SLOW */
	WDMASlowMode(DISP_MODULE_WDMA0, 1, 4, 0xff, 0x7, NULL); /* time period between two write request is 0xff */

	ODDBG(OD_LOG_ALWAYS, "OD SLOW");

	msleep(2000);

	ODDBG(OD_LOG_ALWAYS, "OD OK");
	WDMASlowMode(DISP_MODULE_WDMA0, 0, 0, 0, 0x7, NULL);
#endif
}


static void od_verify_boundary(void)
{
	int guard1, guard2;

	guard1 = (*((u32 *)((unsigned long)g_od_buf.va + g_od_buf.size)) == OD_GUARD_PATTERN);
	guard2 = (*((u32 *)((unsigned long)g_od_buf.va + g_od_buf.size + OD_ADDITIONAL_BUFFER)) == OD_GUARD_PATTERN);

	OD_TLOG("od_verify_boundary(): guard 1 = %d, guard 2 = %d", guard1, guard2);
}


static void od_dump_all(void)
{
	static const unsigned short od_addr_all[] = {
		0x0000, 0x0004, 0x0008, 0x000c, 0x0010, 0x0020, 0x0024, 0x0028, 0x002c, 0x0030,
		0x0040, 0x0044, 0x0048, 0x00c0, 0x0100, 0x0108, 0x010c, 0x0200, 0x0208, 0x020c,
		0x0500, 0x0504, 0x0508, 0x050c, 0x0510, 0x0540, 0x0544, 0x0548, 0x0580, 0x0584,
		0x0588, 0x05c0, 0x05c4, 0x05c8, 0x05cc, 0x05d0, 0x05d4, 0x05d8, 0x05dc, 0x05e0,
		0x05e4, 0x05e8, 0x05ec, 0x05f0, 0x05f8, 0x05fc, 0x0680, 0x0684, 0x0688, 0x068c,
		0x0690, 0x0694, 0x0698, 0x06c0, 0x06c4, 0x06c8, 0x06cc, 0x06d0, 0x06d4, 0x06d8,
		0x06dc, 0x06e0, 0x06e4, 0x06e8, 0x06ec, 0x0700, 0x0704, 0x0708, 0x070c, 0x0710,
		0x0714, 0x0718, 0x071c, 0x0720, 0x0724, 0x0728, 0x072c, 0x0730, 0x0734, 0x0738,
		0x073c, 0x0740, 0x0744, 0x0748, 0x074c, 0x0750, 0x0754, 0x0758, 0x075c, 0x0760,
		0x0764, 0x0768, 0x076c, 0x0770, 0x0774, 0x0778, 0x077c, 0x0788, 0x078c, 0x0790,
		0x0794, 0x0798, 0x079c, 0x07a0, 0x07a4, 0x07a8, 0x07ac, 0x07b0, 0x07b4, 0x07b8,
		0x07bc, 0x07c0, 0x07c4, 0x07c8, 0x07cc, 0x07d0, 0x07d4, 0x07d8, 0x07dc, 0x07e0,
		0x07e4, 0x07e8, 0x07ec
	};
	int i;
	unsigned int value, offset;

	OD_TLOG("OD reg base = %lx", (unsigned long)(OD_BASE));
	for (i = 0; i < sizeof(od_addr_all) / sizeof(od_addr_all[0]); i++) {
		offset = od_addr_all[i];
		value = DISP_REG_GET((unsigned long)(OD_BASE + offset));
		OD_TLOG("[+0x%03x] = 0x%08x(%d)", offset, value, value);
	}
}


static void od_test_stress_table(void *cmdq)
{
	int i;

	ODDBG(OD_LOG_ALWAYS, "OD TEST -- STRESS TABLE START");

	/* read/write table for 100 times, 17x17 and 33x33 50 times each */
	for (i = 0; i < 50; i++) {
		/* test 17 table */
		_od_set_table(cmdq, OD_TABLE_17, OD_Table_17x17, 0);
		_od_read_table(cmdq, OD_TABLE_17, 0, OD_Table_17x17, 0);
		_od_read_table(cmdq, OD_TABLE_17, 1, OD_Table_17x17, 0);
		_od_read_table(cmdq, OD_TABLE_17, 2, OD_Table_17x17, 0);

		/* test 33 table */
		_od_set_table(cmdq, OD_TABLE_33, OD_Table_33x33, 0); /* default use 17x17 table */
		_od_read_table(cmdq, OD_TABLE_33, 0, OD_Table_33x33, 0);
		_od_read_table(cmdq, OD_TABLE_33, 1, OD_Table_33x33, 0);
		_od_read_table(cmdq, OD_TABLE_33, 2, OD_Table_33x33, 0);
	}

	ODDBG(OD_LOG_ALWAYS, "OD TEST -- STRESS TABLE END");
}


void od_test(const char *cmd, char *debug_output)
{
	cmdqRecHandle cmdq;
	unsigned long offset;
	unsigned int value, mask;

	OD_TLOG("od_test(%s)", cmd);

	debug_output[0] = '\0';

	/* Following part does not need cmdq and refresh. */
	if (strncmp(cmd, "log:", 4) == 0) {
		int level = cmd[4] - '0';

		if (OD_LOG_ALWAYS <= level && level <= OD_LOG_DEBUG)
			od_log_level = level;
		return;
	}

	/* Start cmdq to set registers. */
	DISP_CMDQ_BEGIN(cmdq, CMDQ_SCENARIO_DISP_CONFIG_OD);

	if (strncmp(cmd, "set:", 4) == 0) {
		int count = od_parse_triple(cmd + 4, &offset, &value, &mask);

		if (count == 3) {
			DISP_REG_MASK(cmdq, OD_BASE + offset, value, mask);
		} else if (count == 2) {
			DISP_REG_SET(cmdq, OD_BASE + offset, value);
			mask = 0xffffffff;
		}

		if (count >= 2)
			OD_TLOG("[+0x%03lx] = 0x%08x(%d) & 0x%08x", offset, value, value, mask);
	} else if (strncmp(cmd, "dump:", 5) == 0) {
		od_dump_reg(cmd + 5);
	} else if (strncmp(cmd, "dumpall", 7) == 0) {
		od_dump_all();
		ddp_dump_analysis(DISP_MODULE_CONFIG);
	} else if (strncmp(cmd, "stress", 6) == 0) {
		od_test_stress_table(NULL);
	} else if (strncmp(cmd, "slow_mode", 9) == 0) {
		od_test_slow_mode();
	} else if (strncmp(cmd, "boundary", 6) == 0) {
		od_verify_boundary();
#if 0
	} else if (strncmp(cmd, "sodi:", 5) == 0) {
		int enabled = (cmd[5] == '1' ? 1 : 0);

		spm_enable_sodi(enabled);
#endif
	} else if (strncmp(cmd, "en:", 3) == 0) {
		int enabled = (cmd[3] == '1' ? 1 : 0);

		disp_od_set_enabled(cmdq, enabled);
	} else if (strncmp(cmd, "force:", 6) == 0) {
		if (cmd[6] == '0') {
			g_od_debug_enable = DEBUG_ENABLE_NEVER;
			disp_od_set_enabled(cmdq, 0);
			od_refresh_screen();
		} else if (cmd[6] == '1') {
			g_od_debug_enable = DEBUG_ENABLE_ALWAYS;
			disp_od_set_enabled(cmdq, 1);
			disp_od_start_read(cmdq);
			od_refresh_screen();
		} else {
			g_od_debug_enable = DEBUG_ENABLE_NORMAL;
		}
	} else if (strncmp(cmd, "ink:", 4) == 0) {
		if (cmd[4] == '1')
			g_od_debug_mode |= DEBUG_MODE_INK;
		else
			g_od_debug_mode &= ~DEBUG_MODE_INK;
		od_set_debug_function(cmdq);
	} else if (strncmp(cmd, "osd:", 4) == 0) {
		if (cmd[4] == '0')
			g_od_debug_mode = 0;
		else
			g_od_debug_mode = (unsigned int)(cmd[4] - '0');
		od_set_debug_function(cmdq);
	} else if (strncmp(cmd, "demo:", 5) == 0) {
		int enabled = (cmd[5] == '1' ? 1 : 0);

		OD_REG_SET_FIELD(cmdq, OD_REG02, enabled, DEMO_MODE);
		ODDBG(OD_LOG_ALWAYS, "OD demo %d", enabled);
		/* save demo mode flag for suspend/resume */
		g_od_is_demo_mode = enabled;
	} else if (strncmp(cmd, "base", 4) == 0) {
		OD_TLOG("OD reg base = %lx", (unsigned long)(OD_BASE));
	} else if (strncmp(cmd, "data:", 5) == 0) { /* Select output data */
		char mode = cmd[5];

		switch (mode) {
		case '1': /* Bypass OD lookup */
			DISP_REG_MASK(cmdq, OD_REG02, (1 << 9), (0xf << 9));
			break;
		case '2': /* Display decompressed data */
			DISP_REG_MASK(cmdq, OD_REG02, (1 << 12), (0xf << 9));
			break;
		default:
			DISP_REG_MASK(cmdq, OD_REG02, 0, (0xf << 9));
			break;
		}
	}

	DISP_CMDQ_CONFIG_STREAM_DIRTY(cmdq);
	DISP_CMDQ_END(cmdq);

	od_refresh_screen();
}
