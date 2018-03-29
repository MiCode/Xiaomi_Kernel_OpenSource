/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/device.h>       /* needed by device_* */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/io.h>           /* needed by ioremap * */
#include <linux/init.h>         /* needed by module macros */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/module.h>       /* needed by all modules */
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <mt-plat/sync_write.h>
#include <mach/md32_helper.h>
#include <mach/md32_ipi.h>

#include "md32_irq.h"

#define OCD_DEBUG   0
#if OCD_DEBUG
#define ocd_print pr_debug
#else
#define ocd_print(...)
#endif

/* md32 ocd register */
#define MD32_OCD_BYPASS_JTAG_REG   (MD32_BASE + 0x0040)
#define MD32_OCD_INSTR_WR_REG      (MD32_BASE + 0x0044)
#define MD32_OCD_INSTR_REG         (MD32_BASE + 0x0048)
#define MD32_OCD_DATA_WR_REG       (MD32_BASE + 0x004C)
#define MD32_OCD_DATA_PI_REG       (MD32_BASE + 0x0050)
#define MD32_OCD_DATA_PO_REG       (MD32_BASE + 0x0054)
#define MD32_OCD_READY_REG         (MD32_BASE + 0x0058)

#define MD32_OCD_CURRENT_CID	0x1
#define MD32_OCD_CMD(x)		((MD32_OCD_CURRENT_CID << 11) | (x))

#define DBG_DATA_REG_INSTR	0x000
#define DBG_ADDR_REG_INSTR	0x001
#define DBG_INSTR_REG_INSTR	0x002
#define DBG_STATUS_REG_INSTR	0x003
#define DBG_REQUEST_INSTR	0x011
#define DBG_RESUME_INSTR	0x012
#define DBG_RESET_INSTR		0x013
#define DBG_STEP_INSTR		0x014
#define DBG_EXECUTE_INSTR	0x015
#define DBG_BP0_ENABLE_INSTR	0x020
#define DBG_BP0_DISABLE_INSTR	0x022
#define DBG_BP1_ENABLE_INSTR	0x024
#define DBG_BP1_DISABLE_INSTR	0x026
#define DBG_BP2_ENABLE_INSTR	0x028
#define DBG_BP2_DISABLE_INSTR	0x02a
#define DBG_BP3_ENABLE_INSTR	0x02c
#define DBG_BP3_DISABLE_INSTR	0x02e
#define DBG_PMB_LOAD_INSTR	0x040
#define DBG_PMB_STORE_INSTR	0x041
#define DBG_DMB_LOAD_INSTR	0x042
#define DBG_DMB_STORE_INSTR	0x043

#define DBG_MODE_INDX		0
#define DBG_BP_HIT_INDX		5
#define DBG_SWBREAK_INDX	8

enum cmd_md32_ocd {
	CMD_MD32_OCD_STOP = 0,
	CMD_MD32_OCD_RESUME,
	CMD_MD32_OCD_STEP,
	CMD_MD32_OCD_READ_MEM,
	CMD_MD32_OCD_WRITE_MEM,
	CMD_MD32_OCD_WRITE_REG,
	CMD_MD32_OCD_HELP,
	CMD_MD32_OCD_BREAKPOINT,
	CMD_MD32_OCD_STATUS,
	CMD_MD32_OCD_DW,
	CMD_MD32_OCD_DR,
	CMD_MD32_OCD_IW,
	CMD_MD32_OCD_TEST,
	NR_CMD_MD32_OCD
};

struct md32_ocd_cmd_cfg {
	enum cmd_md32_ocd	cmd;
	u32			addr;
	u32			data;
	u32			break_en;
	int			success;
	/* spinlock for read/write ocd cmd register */
	spinlock_t		spinlock;
};

static struct md32_ocd_cmd_cfg md32_ocd_cfg;
#define DM_TMP_ADDR 0x00
enum md32_reg_idx {
	r0 = 0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10,
	r11, r12, r13, r14, r15,
	sr, ipc, isr,
	lf, ls0, le0, lc0, ls1, le1, lc1, ls2, le2, lc2,
	ar0g, ar0h, ar0l, ar1g, ar1h, ar1l,
	srm, b0, b1, m0, m1, l0, l1, o0, o1,
	v0l, v1l, v2l, v3l, v0h, v1h, v2h, v3h,
	rc, pc
};

static const char * const md32_reg_idx_str[] = {
	"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10",
	"r11", "r12", "r13", "r14", "r15",
	"sr", "ipc", "isr",
	"lf", "ls0", "le0", "lc0", "ls1", "le1", "lc1", "ls2", "le2", "lc2",
	"ar0g", "ar0h", "ar0l", "ar1g", "ar1h", "ar1l",
	"srm", "b0", "b1", "m0", "m1", "l0", "l1", "o0", "o1",
	"v0l", "v1l", "v2l", "v3l", "v0h", "v1h", "v2h", "v3h",
	"rc", "pc"
};

void md32_ocd_iw(u32 command)
{
	ocd_print("[MD32 OCD] OCD_READY_REG: read 0x%X = 0x%X\n",
		  MD32_OCD_READY_REG, readl(MD32_OCD_READY_REG));
	while (!readl(MD32_OCD_READY_REG))
		;
	ocd_print("[MD32 OCD] OCD_READY_REG: read 0x%X = 0x%X\n",
		  MD32_OCD_READY_REG, readl(MD32_OCD_READY_REG));

	writel(MD32_OCD_CMD(command), MD32_OCD_INSTR_REG);
	ocd_print("[MD32 OCD] OCD_INSTR_REG: write 0x%X = 0x%X : 0x%X\n",
		  MD32_OCD_INSTR_REG, MD32_OCD_CMD(command),
		  readl(MD32_OCD_INSTR_REG));

	writel(1, MD32_OCD_INSTR_WR_REG);
	ocd_print("[MD32 OCD] OCD_INSTR_WR_REG: write 0x%X = 0x%X : 0x%X\n",
		  MD32_OCD_INSTR_WR_REG, 1, readl(MD32_OCD_INSTR_WR_REG));

	writel(0, MD32_OCD_INSTR_WR_REG);
	ocd_print("[MD32 OCD] OCD_INSTR_WR_REG: write 0x%X = 0x%X : 0x%X\n",
		  MD32_OCD_INSTR_WR_REG, 0, readl(MD32_OCD_INSTR_WR_REG));

	ocd_print("[MD32 OCD] OCD_READY_REG: read 0x%X = 0x%X\n",
		  MD32_OCD_READY_REG, readl(MD32_OCD_READY_REG));
	while (!readl(MD32_OCD_READY_REG))
		;
	ocd_print("[MD32 OCD] OCD_READY_REG: read 0x%X = 0x%X\n",
		  MD32_OCD_READY_REG, readl(MD32_OCD_READY_REG));
}

u32 md32_ocd_dr(u32 reg)
{
	ocd_print("[MD32 OCD] OCD_READY_REG: read 0x%X = 0x%X\n",
		  MD32_OCD_READY_REG, readl(MD32_OCD_READY_REG));
	while (!readl(MD32_OCD_READY_REG))
		;
	ocd_print("[MD32 OCD] OCD_READY_REG: read 0x%X = 0x%X\n",
		  MD32_OCD_READY_REG, readl(MD32_OCD_READY_REG));

	writel(MD32_OCD_CMD(reg), MD32_OCD_INSTR_REG);
	ocd_print("[MD32 OCD] OCD_INSTR_REG: write 0x%X = 0x%X : 0x%X\n",
		  MD32_OCD_INSTR_REG, MD32_OCD_CMD(reg),
		  readl(MD32_OCD_INSTR_REG));

	writel(1, MD32_OCD_INSTR_WR_REG);
	ocd_print("[MD32 OCD] OCD_INSTR_WR_REG: write 0x%X = 0x%X : 0x%X\n",
		  MD32_OCD_INSTR_WR_REG, 1, readl(MD32_OCD_INSTR_WR_REG));

	writel(0, MD32_OCD_INSTR_WR_REG);
	ocd_print("[MD32 OCD] OCD_INSTR_WR_REG: write 0x%X = 0x%X : 0x%X\n",
		  MD32_OCD_INSTR_WR_REG, 0, readl(MD32_OCD_INSTR_WR_REG));

	ocd_print("[MD32 OCD] OCD_READY_REG: read 0x%X = 0x%X\n",
		  MD32_OCD_READY_REG, readl(MD32_OCD_READY_REG));
	while (!readl(MD32_OCD_READY_REG))
		;
	ocd_print("[MD32 OCD] OCD_READY_REG: read 0x%X = 0x%X\n",
		  MD32_OCD_READY_REG, readl(MD32_OCD_READY_REG));

	return readl(MD32_OCD_DATA_PO_REG);
}

void md32_ocd_dw(u32 reg, u32 wdata)
{
	ocd_print("[MD32 OCD] OCD_READY_REG: read 0x%X = 0x%X\n",
		  MD32_OCD_READY_REG, readl(MD32_OCD_READY_REG));
	while (!readl(MD32_OCD_READY_REG))
		;
	ocd_print("[MD32 OCD] OCD_READY_REG: read 0x%X = 0x%X\n",
		  MD32_OCD_READY_REG, readl(MD32_OCD_READY_REG));

	writel(MD32_OCD_CMD(reg), MD32_OCD_INSTR_REG);
	writel(1, MD32_OCD_INSTR_WR_REG);
	ocd_print("[MD32 OCD] OCD_INSTR_WR_REG: write 0x%X = 0x%X : 0x%X\n",
		  MD32_OCD_INSTR_WR_REG, 1, readl(MD32_OCD_INSTR_WR_REG));

	writel(0, MD32_OCD_INSTR_WR_REG);
	ocd_print("[MD32 OCD] OCD_INSTR_WR_REG: write 0x%X = 0x%X : 0x%X\n",
		  MD32_OCD_INSTR_WR_REG, 0, readl(MD32_OCD_INSTR_WR_REG));

	writel(wdata, MD32_OCD_DATA_PI_REG);
	ocd_print("[MD32 OCD] OCD_DATA_PI_REG: write 0x%X = 0x%X : 0x%X\n",
		  MD32_OCD_DATA_PI_REG, wdata, readl(MD32_OCD_DATA_PI_REG));

	writel(1, MD32_OCD_DATA_WR_REG);
	ocd_print("[MD32 OCD] OCD_DATA_WR_REG: write 0x%X = 0x%X : 0x%X\n",
		  MD32_OCD_DATA_WR_REG, 1, readl(MD32_OCD_DATA_WR_REG));

	writel(0, MD32_OCD_DATA_WR_REG);
	ocd_print("[MD32 OCD] OCD_DATA_WR_REG: write 0x%X = 0x%X : 0x%X\n",
		  MD32_OCD_DATA_WR_REG, 0, readl(MD32_OCD_DATA_WR_REG));

	ocd_print("[MD32 OCD] OCD_READY_REG: read 0x%X = 0x%X\n",
		  MD32_OCD_READY_REG, readl(MD32_OCD_READY_REG));
	while (!readl(MD32_OCD_READY_REG))
		;
	ocd_print("[MD32 OCD] OCD_READY_REG: read 0x%X = 0x%X\n",
		  MD32_OCD_READY_REG, readl(MD32_OCD_READY_REG));
}

u32 md32_read_dmw(u32 addr)
{
	u32 data;

	md32_ocd_dw(DBG_ADDR_REG_INSTR, addr);
	md32_ocd_iw(DBG_DMB_LOAD_INSTR);
	*((char *)(&data) + 0) = (char)md32_ocd_dr(DBG_DATA_REG_INSTR);
	md32_ocd_iw(DBG_DMB_LOAD_INSTR);
	*((char *)(&data) + 1) = (char)md32_ocd_dr(DBG_DATA_REG_INSTR);
	md32_ocd_iw(DBG_DMB_LOAD_INSTR);
	*((char *)(&data) + 2) = (char)md32_ocd_dr(DBG_DATA_REG_INSTR);
	md32_ocd_iw(DBG_DMB_LOAD_INSTR);
	*((char *)(&data) + 3) = (char)md32_ocd_dr(DBG_DATA_REG_INSTR);
	return data;
}

void md32_ocd_execute(u32 insn)
{
	while (0 == (md32_ocd_dr(DBG_STATUS_REG_INSTR) & 0x1))
		;
	md32_ocd_dw(DBG_INSTR_REG_INSTR, insn);
	md32_ocd_iw(DBG_EXECUTE_INSTR);
	while (0 == (md32_ocd_dr(DBG_STATUS_REG_INSTR) & 0x1))
		;
}

u32 md32_ocd_read_pmw(u32 addr)
{
	md32_ocd_dw(DBG_ADDR_REG_INSTR, addr);
	md32_ocd_iw(DBG_PMB_LOAD_INSTR);
	return md32_ocd_dr(DBG_INSTR_REG_INSTR);
}

void md32_ocd_write_pmw(u32 addr, u32 wdata)
{
	md32_ocd_dw(DBG_ADDR_REG_INSTR, addr);
	md32_ocd_dw(DBG_INSTR_REG_INSTR, wdata);
	md32_ocd_iw(DBG_PMB_STORE_INSTR);
}

u8 md32_ocd_read_dmb(u32 addr)
{
	md32_ocd_dw(DBG_ADDR_REG_INSTR, addr);
	md32_ocd_iw(DBG_DMB_LOAD_INSTR);
	return md32_ocd_dr(DBG_DATA_REG_INSTR) & 0xff;
}

void md32_ocd_write_dmb(u32 addr, u8 wdata)
{
	md32_ocd_dw(DBG_ADDR_REG_INSTR, addr);
	md32_ocd_dw(DBG_DATA_REG_INSTR, wdata);
	md32_ocd_iw(DBG_DMB_STORE_INSTR);
}

u32 md32_ocd_add_sw_break(u32 addr)
{
	u32 insn = md32_ocd_read_pmw(addr & (~0x3));

	if (addr & 0x2) {	/* unaligned */
		md32_ocd_write_pmw(addr & (~0x3), (insn & 0xffff0000) | 0xa003);
	} else {
		md32_ocd_write_pmw(addr & (~0x3), (insn & 0xffff) | 0xa0030000);
	}
	return insn;
}

static u32 md32_ocd_get_dm_word(u32 addr)
{
	u32 byte0, byte1, byte2, byte3;

	md32_ocd_dw(DBG_ADDR_REG_INSTR, addr);
	md32_ocd_iw(DBG_DMB_LOAD_INSTR);
	byte0 = (md32_ocd_dr(DBG_DATA_REG_INSTR) & 0xff);
	md32_ocd_iw(DBG_DMB_LOAD_INSTR);
	byte1 = (md32_ocd_dr(DBG_DATA_REG_INSTR) & 0xff);
	md32_ocd_iw(DBG_DMB_LOAD_INSTR);
	byte2 = (md32_ocd_dr(DBG_DATA_REG_INSTR) & 0xff);
	md32_ocd_iw(DBG_DMB_LOAD_INSTR);
	byte3 = (md32_ocd_dr(DBG_DATA_REG_INSTR) & 0xff);

	return (byte0 | (byte1 << 8) | (byte2 << 16) | (byte3 << 24));
}

static void md32_ocd_put_dm_word(u32 addr, u32 data)
{
	u32 byte0, byte1, byte2, byte3;

	md32_ocd_dw(DBG_ADDR_REG_INSTR, addr);
	byte0 = (data >> 0) & 0xff;
	byte1 = (data >> 8) & 0xff;
	byte2 = (data >> 16) & 0xff;
	byte3 = (data >> 24) & 0xff;
	md32_ocd_dw(DBG_DATA_REG_INSTR, byte0);
	md32_ocd_iw(DBG_DMB_STORE_INSTR);
	md32_ocd_dw(DBG_DATA_REG_INSTR, byte1);
	md32_ocd_iw(DBG_DMB_STORE_INSTR);
	md32_ocd_dw(DBG_DATA_REG_INSTR, byte2);
	md32_ocd_iw(DBG_DMB_STORE_INSTR);
	md32_ocd_dw(DBG_DATA_REG_INSTR, byte3);
	md32_ocd_iw(DBG_DMB_STORE_INSTR);
}

static inline void md32_ocd_set_r0(u32 data)
{
	if ((data >> 20) == 0) {
		md32_ocd_execute(0x00000000 | (data << 4));
	} else {
		u32 hi = (data >> 16);
		u32 lo = (data & 0xffff);

		md32_ocd_execute(0x0f000000 | (hi << 8));
		if (lo != 0)
			md32_ocd_execute(0x0d000000 | (lo << 8));
	}
}

static inline void md32_ocd_set_r1(u32 data)
{
	if ((data >> 20) == 0) {
		md32_ocd_execute(0x00000001 | (data << 4));
	} else {
		u32 hi = (data >> 16);
		u32 lo = (data & 0xffff);

		md32_ocd_execute(0x0f000001 | (hi << 8));
		if (lo != 0)
			md32_ocd_execute(0x0d000011 | (lo << 8));
	}
}

inline u32 md32_ocd_read_pc(void)
{
	return md32_ocd_dr(DBG_ADDR_REG_INSTR);
}

void md32_ocd_write_pc(u32 addr)
{
	/* setup PC to R0 */
	md32_ocd_set_r0(addr);
	/* jump to r0 */
	md32_ocd_execute(0x40002030);
	/* restore R0 */
	md32_ocd_execute(0x00000000);
}

u32 md32_ocd_read_reg(enum md32_reg_idx r)
{
	u32 dm_tmp_bak = 0;
	u32 r1_tmp_bak = 0;
	u32 ret = 0;
	int ret_from_dm = 1;

	/* setup DM_TMP_ADDR */
	md32_ocd_set_r0(DM_TMP_ADDR);
	/* backup DM_TMP */
	dm_tmp_bak = md32_ocd_get_dm_word(DM_TMP_ADDR);
	/* backup R1 */
	md32_ocd_execute(0x06700010);
	r1_tmp_bak = md32_ocd_get_dm_word(DM_TMP_ADDR);

	switch (r) {
	case r0:
		ret = 0;
		ret_from_dm = 0;
		break;
	case r1:
		ret = r1_tmp_bak;
		ret_from_dm = 0;
		break;
	case r2:
		md32_ocd_execute(0x06700020);
		break;
	case r3:
		md32_ocd_execute(0x06700030);
		break;
	case r4:
		md32_ocd_execute(0x06700040);
		break;
	case r5:
		md32_ocd_execute(0x06700050);
		break;
	case r6:
		md32_ocd_execute(0x06700060);
		break;
	case r7:
		md32_ocd_execute(0x06700070);
		break;
	case r8:
		md32_ocd_execute(0x06700080);
		break;
	case r9:
		md32_ocd_execute(0x06700090);
		break;
	case r10:
		md32_ocd_execute(0x067000a0);
		break;
	case r11:
		md32_ocd_execute(0x067000b0);
		break;
	case r12:
		md32_ocd_execute(0x067000c0);
		break;
	case r13:
		md32_ocd_execute(0x067000d0);
		break;
	case r14:
		md32_ocd_execute(0x067000e0);
		break;
	case r15:
		md32_ocd_execute(0x067000f0);
		break;
	default:
		ret = 0xdeadbeef;
		ret_from_dm = 0;
	}
	if (ret_from_dm)
		ret = md32_ocd_get_dm_word(DM_TMP_ADDR);
	/* restore R0 */
	md32_ocd_execute(0x00000000);
	/* restore R1 */
	md32_ocd_set_r1(r1_tmp_bak);
	/* restore DM_TMP */
	md32_ocd_put_dm_word(DM_TMP_ADDR, dm_tmp_bak);
	return ret;
}

static inline void md32_ocd_set_vr(const enum md32_reg_idx r, u32 data)
{
	u32 dm_tmp_bak = 0;
	u32 r1_tmp_bak = 0;

	md32_ocd_set_r0(DM_TMP_ADDR);
	dm_tmp_bak = md32_ocd_get_dm_word(DM_TMP_ADDR);
	md32_ocd_execute(0x06700010);
	r1_tmp_bak = md32_ocd_get_dm_word(DM_TMP_ADDR);
	switch (r) {
	case v0l:
		md32_ocd_execute(0x20001810);
		md32_ocd_set_r0(data);
		md32_ocd_execute(0x20001c10);
		break;
	case v1l:
		md32_ocd_execute(0x20001910);
		md32_ocd_set_r0(data);
		md32_ocd_execute(0x20001d10);
		break;
	case v2l:
		md32_ocd_execute(0x20001a10);
		md32_ocd_set_r0(data);
		md32_ocd_execute(0x20001e10);
		break;
	case v3l:
		md32_ocd_execute(0x20001b10);
		md32_ocd_set_r0(data);
		md32_ocd_execute(0x20001f10);
		break;
	case v0h:
		md32_ocd_execute(0x20001810);
		md32_ocd_set_r1(data);
		md32_ocd_execute(0x20001c10);
		break;
	case v1h:
		md32_ocd_execute(0x20001910);
		md32_ocd_set_r1(data);
		md32_ocd_execute(0x20001d10);
		break;
	case v2h:
		md32_ocd_execute(0x20001a10);
		md32_ocd_set_r1(data);
		md32_ocd_execute(0x20001e10);
		break;
	case v3h:
		md32_ocd_execute(0x20001b10);
		md32_ocd_set_r1(data);
		md32_ocd_execute(0x20001f10);
		break;
	default:
		break;
	}
	/* Restore R0 */
	md32_ocd_execute(0x00000000);
	md32_ocd_set_r1(r1_tmp_bak);
	md32_ocd_put_dm_word(DM_TMP_ADDR, dm_tmp_bak);
}

void md32_ocd_write_reg(enum md32_reg_idx r, u32 data)
{
	/* write R0 */
	md32_ocd_set_r0(data);
	switch (r) {
	case r0:
		break;
	case r1:
		md32_ocd_execute(0x08000001);
		break;
	case r2:
		md32_ocd_execute(0x08000002);
		break;
	case r3:
		md32_ocd_execute(0x08000003);
		break;
	case r4:
		md32_ocd_execute(0x08000004);
		break;
	case r5:
		md32_ocd_execute(0x08000005);
		break;
	case r6:
		md32_ocd_execute(0x08000006);
		break;
	case r7:
		md32_ocd_execute(0x08000007);
		break;
	case r8:
		md32_ocd_execute(0x08000008);
		break;
	case r9:
		md32_ocd_execute(0x08000009);
		break;
	case r10:
		md32_ocd_execute(0x0800000a);
		break;
	case r11:
		md32_ocd_execute(0x0800000b);
		break;
	case r12:
		md32_ocd_execute(0x0800000c);
		break;
	case r13:
		md32_ocd_execute(0x0800000d);
		break;
	case r14:
		md32_ocd_execute(0x0800000e);
		break;
	case r15:
		md32_ocd_execute(0x0800000f);
		break;
	case lf:
		md32_ocd_execute(0x04182004);
		break;
	case ls0:
		md32_ocd_execute(0x04182005);
		break;
	case le0:
		md32_ocd_execute(0x04182006);
		break;
	case lc0:
		md32_ocd_execute(0x04182007);
		break;
	case ls1:
		md32_ocd_execute(0x04182008);
		break;
	case le1:
		md32_ocd_execute(0x04182009);
		break;
	case lc1:
		md32_ocd_execute(0x0418200a);
		break;
	case ls2:
		md32_ocd_execute(0x0418200b);
		break;
	case le2:
		md32_ocd_execute(0x0418200c);
		break;
	case lc2:
		md32_ocd_execute(0x0418200d);
		break;
	case ipc:
		md32_ocd_execute(0x04182001);
		break;
	case isr:
		md32_ocd_execute(0x04182002);
		break;
	case sr:
		md32_ocd_execute(0x04182000);
		break;
	case srm:
		md32_ocd_execute(0x40006010);
		break;
	case m0:
		md32_ocd_execute(0x400060a0);
		break;
	case m1:
		md32_ocd_execute(0x400060b0);
		break;
	case b0:
		md32_ocd_execute(0x40006080);
		break;
	case b1:
		md32_ocd_execute(0x40006090);
		break;
	case o0:
		md32_ocd_execute(0x400060e0);
		break;
	case o1:
		md32_ocd_execute(0x400060f0);
		break;
	case l0:
		md32_ocd_execute(0x400060c0);
		break;
	case l1:
		md32_ocd_execute(0x400060d0);
		break;
	case v0l:
	case v1l:
	case v2l:
	case v3l:
	case v0h:
	case v1h:
	case v2h:
	case v3h:
		md32_ocd_set_vr(r, data);
		break;
	case rc: /* do nothing */
		break;
	case pc:
		md32_ocd_execute(0x40002030);
		break;
	default:
		break;
	}
	/* restore R0 */
	md32_ocd_execute(0x00000000);
}

u32 md32_ocd_read_mmr(u32 addr)
{
	u32 dm_tmp_bak = 0;
	u32 r1_tmp_bak = 0;
	u32 ret = 0;

	md32_ocd_set_r0(DM_TMP_ADDR);
	dm_tmp_bak = md32_ocd_get_dm_word(DM_TMP_ADDR);
	md32_ocd_execute(0x06700010);
	r1_tmp_bak = md32_ocd_get_dm_word(DM_TMP_ADDR);

	md32_ocd_set_r0(addr & (~0x3));
	md32_ocd_execute(0x06400010); /* lw r1,#0(r0) */
	md32_ocd_set_r0(DM_TMP_ADDR);
	md32_ocd_execute(0x06700010);
	ret = md32_ocd_get_dm_word(DM_TMP_ADDR);

	/* restore R0 */
	md32_ocd_execute(0x00000000);
	md32_ocd_set_r1(r1_tmp_bak);
	md32_ocd_put_dm_word(DM_TMP_ADDR, dm_tmp_bak);
	return ret;
}

void md32_ocd_write_mmr(u32 addr, u32 data)
{
	u32 dm_tmp_bak = 0;
	u32 r1_tmp_bak = 0;

	md32_ocd_set_r0(DM_TMP_ADDR);
	dm_tmp_bak = md32_ocd_get_dm_word(DM_TMP_ADDR);
	md32_ocd_execute(0x06700010);
	r1_tmp_bak = md32_ocd_get_dm_word(DM_TMP_ADDR);

	md32_ocd_set_r0(addr & (~0x3));
	md32_ocd_set_r1(data);
	md32_ocd_execute(0x06700010);

	/* restore R0 */
	md32_ocd_execute(0x00000000);
	md32_ocd_set_r1(r1_tmp_bak);
	md32_ocd_put_dm_word(DM_TMP_ADDR, dm_tmp_bak);
}

int md32_ocd_set_break(u32 enable, u32 num, u32 addr)
{
	int err = 0;
	u32 command = 0;

	if (enable) {
		/* enable break pointer */
		switch (num) {
		case 0:
			command = DBG_BP0_ENABLE_INSTR;
			break;
		case 1:
			command = DBG_BP1_ENABLE_INSTR;
			break;
		case 2:
			command = DBG_BP2_ENABLE_INSTR;
			break;
		case 3:
			command = DBG_BP3_ENABLE_INSTR;
			break;

		default:
			err = 1;
			goto error;
		}
		md32_ocd_dw(DBG_ADDR_REG_INSTR, addr);
		md32_ocd_iw(command);
	} else {
		/* disable break pointer */
		switch (num) {
		case 0:
			command = DBG_BP0_DISABLE_INSTR;
			break;
		case 1:
			command = DBG_BP1_DISABLE_INSTR;
			break;
		case 2:
			command = DBG_BP2_DISABLE_INSTR;
			break;
		case 3:
			command = DBG_BP3_DISABLE_INSTR;
			break;

		default:
			err = 1;
			goto error;
		}
		md32_ocd_iw(command);
	}

error:
	return err;
}

ssize_t md32_ocd_dump_all_cpu_reg(char *buf)
{
	char *ptr = buf;
	int i;

	if (!buf)
		return 0;

	for (i = r14; i <= r15; i++) {
		ptr += sprintf(ptr, "%s=0x%08x\n", md32_reg_idx_str[i],
			       md32_ocd_read_reg(i));
	}
	ptr += sprintf(ptr, "dump md32 debug reg\n");
	ptr += sprintf(ptr, "pc=0x%08x, r14=0x%08x, r15=0x%08x\n",
		       readl(MD32_DEBUG_PC_REG), readl(MD32_DEBUG_R14_REG),
		       readl(MD32_DEBUG_R15_REG));

	return ptr - buf;
}

int md32_ocd_execute_cmd(struct md32_ocd_cmd_cfg *in_cfg)
{
	struct md32_ocd_cmd_cfg *cfg = &md32_ocd_cfg;
	unsigned long irq_flag = 0;

	pr_debug("%s: cmd:%d, addr=0x%x, data=0x%x\n", __func__, in_cfg->cmd,
		 in_cfg->addr, in_cfg->data);

	spin_lock_irqsave(&cfg->spinlock, irq_flag);

	ocd_print("[MD32 OCD] MD32_BASE: 0x%X\n", MD32_BASE);

	writel(0x1, MD32_OCD_BYPASS_JTAG_REG);
	ocd_print("[MD32 OCD] OCD_BYPASS_JTAG_REG: write 0x%X = 0x%X : 0x%X\n",
		  MD32_OCD_BYPASS_JTAG_REG, 0x1, MD32_OCD_BYPASS_JTAG_REG);

	cfg->cmd  = in_cfg->cmd;
	cfg->addr = in_cfg->addr;
	cfg->data = in_cfg->data;
	cfg->break_en = in_cfg->break_en;
	cfg->success = 0;

	switch (cfg->cmd) {
	case CMD_MD32_OCD_STOP:
		md32_ocd_iw(DBG_REQUEST_INSTR);
		break;

	case CMD_MD32_OCD_RESUME:
		md32_ocd_iw(DBG_RESUME_INSTR);
		break;

	case CMD_MD32_OCD_STEP:
		md32_ocd_iw(DBG_STEP_INSTR);
		break;

	case CMD_MD32_OCD_READ_MEM:
		cfg->data = md32_ocd_get_dm_word(cfg->addr);
		break;

	case CMD_MD32_OCD_WRITE_MEM:
		md32_ocd_put_dm_word(cfg->addr, cfg->data);
		break;

	case CMD_MD32_OCD_BREAKPOINT:
		md32_ocd_set_break(cfg->break_en, cfg->data, cfg->addr);
		break;

	case CMD_MD32_OCD_STATUS:
		cfg->data = md32_ocd_dr(DBG_STATUS_REG_INSTR);
		break;

	case CMD_MD32_OCD_DW:
		md32_ocd_dw(cfg->addr, cfg->data);
		break;
	case CMD_MD32_OCD_IW:
		md32_ocd_iw(cfg->addr);
		break;
	case CMD_MD32_OCD_DR:
		cfg->data = md32_ocd_dr(cfg->addr);
		break;

	case CMD_MD32_OCD_TEST:
		pr_debug("\n\n\ndr(DBG_STATUS_REG_INSTR)=0x%x\n",
			 md32_ocd_dr(DBG_STATUS_REG_INSTR));
		md32_ocd_dw(DBG_DATA_REG_INSTR, 0xAB);
		cfg->data = md32_ocd_dr(DBG_DATA_REG_INSTR);
		pr_debug("dr(DBG_DATA_REG_INSTR)=0x%x\n", cfg->data);
		break;

	default:
		pr_err("unkonw md32 ocd cmd\n");
		break;
	}

	cfg->success = 1;

	writel(0x0, MD32_OCD_BYPASS_JTAG_REG);
	ocd_print("[MD32 OCD] OCD_BYPASS_JTAG_REG: write 0x%X = 0x%X : 0x%X\n",
		  MD32_OCD_BYPASS_JTAG_REG, 0x0,
		  readl(MD32_OCD_BYPASS_JTAG_REG));

	spin_unlock_irqrestore(&cfg->spinlock, irq_flag);
	pr_debug("%s, done\n", __func__);

	return 0;
}

const char *md32_ocd_help_msg(void)
{
	static const char help_str[] = {
		"md32 ocd command usage:\n"
		"echo [cmd] [address] [data] > md32_ocd\n\n"
		"[cmd]\n"
		"\tstop:   stop md32\n"
		"\tresume: resume md32\n"
		"\tstep:   step md32\n"
		"\twrite:  use md32 to write address with data\n"
		"\t        write [address] [data]\n"
		"\tread:   use md32 to read address\n"
		"\t        read [address]\n"
		"\tbreak:  setup break pointer\n"
		"\t        break [enable/disable] [0-3] [address]\n"
		"\tstatus: show status\n"
		"\tdw:     dw [reg] [data]\n"
		"\tiw:     iw [reg]\n"
		"\tdr:     dr [reg]\n"
		"\t        use cat to get return data\n"
		"\thelp:   show usage\n"
		"\n"
		"ex: <write>\n"
		"echo write 0xD0000084 0x00001234 > md32_ocd\n\n"
		"ex: <dw, iw, dr> to read address 0x00000000\n"
		"dw 0x001 0x00000000\n" "iw 0x042\n" "dr 0x000\n"
	};

	return help_str;
}

int md32_ocd_input_parse(const char *buf, size_t n,
			 struct md32_ocd_cmd_cfg *cfg)
{
	u32 addr = 0, data = 0, break_en = 0;
	enum cmd_md32_ocd cmd = NR_CMD_MD32_OCD;
	char cmd_str[64], cmd_str2[64];
	int err = 0;
	u32 res = sscanf(buf, "%s 0x%x 0x%x", cmd_str, &addr, &data);

	if (res < 1) {
		err = 1;
		goto error;
	}

	switch (cmd_str[0]) {
	case 'b':
		res = sscanf(buf, "%s %s %x 0x%x", cmd_str, cmd_str2, &data,
			     &addr);
		if (res > 4 || data > 3) {
			err = 1;
			goto error;
		}

		break_en = cmd_str2[0] == 'e' ? 1 : 0; /* string is 'enable' */
		cmd = CMD_MD32_OCD_BREAKPOINT;
		break;

	case 'i':
		if (strncmp(cmd_str, "iw", 2) == 0) {
			res = sscanf(buf, "%s 0x%x", cmd_str, &addr);
			if (res != 2) {
				err = 1;
				goto error;
			}

			cmd = CMD_MD32_OCD_IW;
		}
		break;

	case 'd':
		if (strncmp(cmd_str, "dw", 2) == 0) {
			res = sscanf(buf, "%s 0x%x 0x%x", cmd_str, &addr,
				     &data);
			if (res != 3) {
				err = 1;
				goto error;
			}

			cmd = CMD_MD32_OCD_DW;
		} else if (strncmp(cmd_str, "dr", 2) == 0) {
			res = sscanf(buf, "%s 0x%x", cmd_str, &addr);
			if (res != 2) {
				err = 1;
				goto error;
			}
			cmd = CMD_MD32_OCD_DR;
		}
		break;

	case 's':
		if (strncmp(cmd_str, "stop", 4) == 0) {
			cmd = CMD_MD32_OCD_STOP;
		} else if (strncmp(cmd_str, "status", 6) == 0) {
			cmd = CMD_MD32_OCD_STATUS;
		} else {
			/*step */
			cmd = CMD_MD32_OCD_STEP;
		}
		break;

	case 'r':
		if (strncmp(cmd_str, "read", 4) == 0) {
			res = sscanf(buf, "%s 0x%x", cmd_str, &addr);
			if (res != 2) {
				err = 1;
				goto error;
			}
			cmd = CMD_MD32_OCD_READ_MEM;
		} else {
			cmd = CMD_MD32_OCD_RESUME;
		}
		break;

	case 'h':
		cmd = CMD_MD32_OCD_HELP;
		break;

	case 'w':
		res = sscanf(buf, "%s 0x%x 0x%x", cmd_str, &addr, &data);
		if (res != 3) {
			err = 1;
			goto error;
		}
		cmd = CMD_MD32_OCD_WRITE_MEM;

		break;
	case 't':
		cmd = CMD_MD32_OCD_TEST;
		break;

	default:
		err = 1;
		break;
	}

	if (cmd == NR_CMD_MD32_OCD)
		err = 1;

	if (!err) {
		cfg->cmd = cmd;
		cfg->addr = addr;
		cfg->data = data;
		cfg->break_en = break_en;
	}

error:
	return err;
}

static ssize_t md32_ocd_show(struct device *kobj,
			     struct device_attribute *attr, char *buf)
{
	int size = 0;
	char *ptr = buf;
	unsigned long irq_flag;
	struct md32_ocd_cmd_cfg *cfg = &md32_ocd_cfg;
	unsigned int sw_rstn;

	sw_rstn = readl((void __iomem *)MD32_BASE);

	if (sw_rstn == 0x0) {
		ptr += sprintf(ptr, "MD32 not enabled\n");
		goto error;
	}

	spin_lock_irqsave(&cfg->spinlock, irq_flag);

	ptr += sprintf(ptr, "[%s] ", cfg->success ? "OK" : "FAIL");

	ptr += sprintf(ptr, "command: ");

	switch (cfg->cmd) {
	case CMD_MD32_OCD_STOP:
		ptr += sprintf(ptr, "stop");
		break;

	case CMD_MD32_OCD_STEP:
		ptr += sprintf(ptr, "step");
		break;

	case CMD_MD32_OCD_RESUME:
		ptr += sprintf(ptr, "resume");
		break;

	case CMD_MD32_OCD_HELP:
		ptr += sprintf(ptr, "help\n");
		ptr += sprintf(ptr, "%s", md32_ocd_help_msg());
		break;

	case CMD_MD32_OCD_WRITE_MEM:
		ptr += sprintf(ptr, "write addr=0x%08x, data=0x%08x",
			       cfg->addr, cfg->data);
		break;

	case CMD_MD32_OCD_READ_MEM:
		ptr += sprintf(ptr, "read addr=0x%08x, data=0x%08x",
			       cfg->addr, cfg->data);
		break;
	case CMD_MD32_OCD_TEST:
		ptr += sprintf(ptr, "test addr=0x%08x, data=0x%08x",
			       cfg->addr, cfg->data);
		break;

	case CMD_MD32_OCD_BREAKPOINT:
		ptr += sprintf(ptr, "break");
		if (cfg->break_en) {
			ptr += sprintf(ptr, "enable %d addr=0x%08x",
				       cfg->data, cfg->addr);
		} else {
			ptr += sprintf(ptr, "disable %d", cfg->data);
		}

		break;

	case CMD_MD32_OCD_STATUS:
		ptr += sprintf(ptr, "status data=0x%08x\n", cfg->data);

		if (0 == (cfg->data & (0x1 << DBG_MODE_INDX))) {
			ptr += sprintf(ptr, "md32 is running\n");
		} else {
			if (0 != (cfg->data & (0x1 << (DBG_MODE_INDX + 1 + 0))))
				ptr += sprintf(ptr,
					       "hit hardware breakpoint 0\n");
			if (0 != (cfg->data & (0x1 << (DBG_MODE_INDX + 1 + 1))))
				ptr += sprintf(ptr,
					       "hit hardware breakpoint 1\n");
			if (0 != (cfg->data & (0x1 << (DBG_MODE_INDX + 1 + 2))))
				ptr += sprintf(ptr,
					       "hit hardware breakpoint 2\n");
			if (0 != (cfg->data & (0x1 << (DBG_MODE_INDX + 1 + 3))))
				ptr += sprintf(ptr,
					       "hit hardware breakpoint 3\n");
			if (0 != (cfg->data & (0x1 << DBG_BP_HIT_INDX)))
				ptr += sprintf(ptr,
					       "hit hardware breakpoint\n");
			if (0 != (cfg->data & (0x1 << DBG_SWBREAK_INDX)))
				ptr += sprintf(ptr,
					       "hit software breakpoint\n");

			ptr += md32_ocd_dump_all_cpu_reg(ptr);
		}
		break;
	case CMD_MD32_OCD_DW:
		ptr += sprintf(ptr, "dw reg=0x%08x, data=0x%08x", cfg->addr,
			       cfg->data);
		break;

	case CMD_MD32_OCD_DR:
		ptr += sprintf(ptr, "dr reg=0x%08x, data=0x%08x",
			       cfg->addr, cfg->data);
		break;

	case CMD_MD32_OCD_IW:
		ptr += sprintf(ptr, "iw reg=0x%08x", cfg->addr);
		break;

	default:
		ptr += sprintf(ptr, "unknown");
		break;
	}

	ptr += sprintf(ptr, "\n");

	spin_unlock_irqrestore(&cfg->spinlock, irq_flag);

error:
	size = ptr - buf;
	return size;
}

static ssize_t md32_ocd_store(struct device *kobj,
			      struct device_attribute *attr,
			      const char *buf, size_t n)
{
	struct md32_ocd_cmd_cfg cfg;
	unsigned int sw_rstn;

	sw_rstn = readl((void __iomem *)MD32_BASE);

	if (sw_rstn == 0x0) {
		pr_err("MD32 not enabled\n");
		return -EINVAL;
	}

	if (md32_ocd_input_parse(buf, n, &cfg)) {
		pr_debug("%s", md32_ocd_help_msg());
		return -EINVAL;
	}

	if (md32_ocd_execute_cmd(&cfg)) {
		pr_err("ocd execute fail\n");
		return -EBUSY;
	}

	pr_debug("\nmd32_ocd_store end\n\n\n");

	return n;
}

DEVICE_ATTR(md32_ocd, 0644, md32_ocd_show, md32_ocd_store);

void md32_ocd_init(void)
{
	spin_lock_init(&md32_ocd_cfg.spinlock);
}
