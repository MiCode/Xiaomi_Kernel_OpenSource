// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 */
#include "ccci_core.h"
#include "ccci_platform.h"

#include "md_sys1_platform.h"
#include "cldma_reg.h"
#include "modem_reg_base.h"
#include "modem_secure_base.h"
#include "ap_md_reg_dump.h"

#define TAG "mcd"

#define RAnd2W(a, b, c)  ccci_write32(a, b, (ccci_read32(a, b)&c))

/*
 * This file is generated.
 * From 20181114_Latife_MDReg_remap.xlsx
 * With ap_md_reg_dump_code_gentool.py v0.1
 * Date 2018-11-14 13:02:08.882000
 */
static void internal_md_dump_debug_register(unsigned int md_index)
{
	void __iomem *dump_reg0;

	/* dump AP_MDSRC_REQ */
	dump_reg0 = ioremap_nocache(0x10006434, 0x4);
	if (dump_reg0 == NULL) {
		CCCI_MEM_LOG_TAG(md_index, TAG,
			"Dump MD failed to ioremap 0x4 bytes from 0x10006434\n");
		return;
	}
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"md_dbg_sys: 0x%X\n", ccci_read32(dump_reg0, 0x0));
	iounmap(dump_reg0);

	/* PC Monitor */
	dump_reg0 = ioremap_nocache(0x0D0D9000, 0x1360);
	if (dump_reg0 == NULL) {
		CCCI_MEM_LOG_TAG(md_index, TAG,
			"Dump MD failed to ioremap 0x1360 bytes from 0x0D0D9000\n");
		return;
	}
	/* Stop PCMon */
	mdreg_write32(MD_REG_PC_MONITOR, 0x222);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD PC monitor\n");
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"common: 0x0D0DA000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00001000), 0x100);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00001100), 0x60);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00001200), 0x60);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00001300), 0x60);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"core0/1/2: [0]0x0D0D9000, [1]0x0D0D9400, [2]0x0D0D9800\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000000), 0x400);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000400), 0x400);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000800), 0x400);
	/* Re-Start PCMon */
	mdreg_write32(MD_REG_PC_MONITOR, 0x111);
	iounmap(dump_reg0);

	/* PLL reg (clock control) */
	dump_reg0 = ioremap_nocache(0x0D0C3800, 0x1C85C);
	if (dump_reg0 == NULL) {
		CCCI_MEM_LOG_TAG(md_index, TAG,
			"Dump MD failed to ioremap 0x1C85C bytes from 0x0D0C3800\n");
		return;
	}
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD PLL\n");
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"CLKSW: [0]0x0D0D6000, [1]0x0D0D6200, [2]0x0D0D6F00\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00012800), 0x110);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00012A00), 0x20);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00013700), 0x8);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"PLLMIXED:[0]0x0D0D4000,[1]0x0D0D4100,[2]0x0D0D4200,[3]0x0D0D4300,[4]0x0D0D4400,[5]0x0D0D4500,[6]0x0D0D4C00,[7]0x0D0D4D00,[8]0x0D0D4F00\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00010800), 0x68);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00010900), 0x30);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00010A00), 0x8);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00010B00), 0x20);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00010C00), 0x60);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00010D00), 0xD0);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00011400), 0x48);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00011500), 0x8);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00011700), 0x14);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"CLKCTL: [0]0x0D0C3800, [1]0x0D0C3910\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000000), 0x1C);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000110), 0x20);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"GLOBAL CON: [0]0x0D0D5000, [1]0x0D0D5090, [2]0x0D0D5200, [3]0x0D0D5300, [4]0x0D0D5700, [5]0x0D0D5800, [6]0x0D0D5900, [7]0x0D0D5D00, [8]0x0D0D5F00\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00011800), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00011890), 0x80);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00011A00), 0x80);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00011B00), 0x70);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00011F00), 0x50);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00012000), 0x30);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00012100), 0x8);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00012500), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00012700), 0x8);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"AO CONFIG: [0]0x0D0E0050\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x0001C850), 0xC);
	iounmap(dump_reg0);

	/* BUS */
	dump_reg0 = ioremap_nocache(0x0D0C7000, 0x19098);
	if (dump_reg0 == NULL) {
		CCCI_MEM_LOG_TAG(md_index, TAG,
			"Dump MD failed to ioremap 0x19098 bytes from 0x0D0C7000\n");
		return;
	}
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD Bus status: [0]0x0D0C7000, [1]0x0D0C9000, [2]0x0D0E0000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000000), 0xE0);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002000), 0x110);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00019000), 0x98);
	iounmap(dump_reg0);

	/* BUSMON  */
	dump_reg0 = ioremap_nocache(0x0D0C6000, 0x291C);
	if (dump_reg0 == NULL) {
		CCCI_MEM_LOG_TAG(md_index, TAG,
			"Dump MD failed to ioremap 0x291C bytes from 0x0D0C6000\n");
		return;
	}
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD Bus REC: [0]0x0D0C6000, [1]0x0D0C8000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000000), 0x104);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000200), 0x1C);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000220), 0x30);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000280), 0x1C);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x000002A0), 0x30);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000400), 0x51C);
	/* [Pre-Action] Disable bus his rec & select entry 0 */
	mdreg_write32(MD_REG_MDMCU_BUSMON, 0x0);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000860), 0xC);
	/* [Pre-Action] Select entry 1 */
	mdreg_write32(MD_REG_MDMCU_BUSMON, 0x100010);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000860), 0xC);
	/* [Pre-Action] Select entry 2 */
	mdreg_write32(MD_REG_MDMCU_BUSMON, 0x200020);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000860), 0xC);
	/* [Pre-Action] Select entry 3 */
	mdreg_write32(MD_REG_MDMCU_BUSMON, 0x300030);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000860), 0xC);
	/* [Pre-Action] Select entry 4 */
	mdreg_write32(MD_REG_MDMCU_BUSMON, 0x400040);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000860), 0xC);
	/* [Pre-Action] Select entry 5 */
	mdreg_write32(MD_REG_MDMCU_BUSMON, 0x500050);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000860), 0xC);
	/* [Pre-Action] Select entry 6 */
	mdreg_write32(MD_REG_MDMCU_BUSMON, 0x600060);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000860), 0xC);
	/* [Pre-Action] Select entry 7 */
	mdreg_write32(MD_REG_MDMCU_BUSMON, 0x700070);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000860), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002000), 0x104);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002200), 0x1C);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002220), 0x30);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002280), 0x1C);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x000022A0), 0x30);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002400), 0x51C);
	/* [Pre-Action] Disable bus his rec & select entry 0 */
	mdreg_write32(MD_REG_MDINFRA_BUSMON, 0x0);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002860), 0xC);
	/* [Pre-Action] Select entry 1 */
	mdreg_write32(MD_REG_MDINFRA_BUSMON, 0x100010);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002860), 0xC);
	/* [Pre-Action] Select entry 2 */
	mdreg_write32(MD_REG_MDINFRA_BUSMON, 0x200020);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002860), 0xC);
	/* [Pre-Action] Select entry 3 */
	mdreg_write32(MD_REG_MDINFRA_BUSMON, 0x300030);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002860), 0xC);
	/* [Pre-Action] Select entry 4 */
	mdreg_write32(MD_REG_MDINFRA_BUSMON, 0x400040);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002860), 0xC);
	/* [Pre-Action] Select entry 5 */
	mdreg_write32(MD_REG_MDINFRA_BUSMON, 0x500050);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002860), 0xC);
	/* [Pre-Action] Select entry 6 */
	mdreg_write32(MD_REG_MDINFRA_BUSMON, 0x600060);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002860), 0xC);
	/* [Pre-Action] Select entry 7 */
	mdreg_write32(MD_REG_MDINFRA_BUSMON, 0x700070);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00002860), 0xC);
	iounmap(dump_reg0);

	/* ECT */
	dump_reg0 = ioremap_nocache(0x0D0CC130, 0x1EE8);
	if (dump_reg0 == NULL) {
		CCCI_MEM_LOG_TAG(md_index, TAG,
			"Dump MD failed to ioremap 0x1EE8 bytes from 0x0D0CC130\n");
		return;
	}
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD ECT: [0]0x0D0CC130, [1]0x0D0CC134, [2]0x0D0CD130, [3]0x0D0CD134, [4]0x0D0CE014, [5]0x0D0CE00C\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000000), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000004), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00001000), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00001004), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00001EE4), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00001EDC), 0x4);
	iounmap(dump_reg0);

	/* TOPSM reg */
	dump_reg0 = ioremap_nocache(0x0D0D0000, 0x8E4);
	if (dump_reg0 == NULL) {
		CCCI_MEM_LOG_TAG(md_index, TAG,
			"Dump MD failed to ioremap 0x8E4 bytes from 0x0D0D0000\n");
		return;
	}
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD TOPSM status: 0x0D0D0000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000000), 0x8E4);
	iounmap(dump_reg0);

	/* MD RGU reg */
	dump_reg0 = ioremap_nocache(0x0D0D2100, 0x25C);
	if (dump_reg0 == NULL) {
		CCCI_MEM_LOG_TAG(md_index, TAG,
			"Dump MD failed to ioremap 0x25C bytes from 0x0D0D2100\n");
		return;
	}
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD RGU: [0]0x0D0D2100, [1]0x0D0D2300\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000000), 0xCC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000200), 0x5C);
	iounmap(dump_reg0);

	/* OST status */
	dump_reg0 = ioremap_nocache(0x0D0D1000, 0x208);
	if (dump_reg0 == NULL) {
		CCCI_MEM_LOG_TAG(md_index, TAG,
			"Dump MD failed to ioremap 0x208 bytes from 0x0D0D1000\n");
		return;
	}
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD OST status: [0]0x0D0D1000, [1]0x0D0D1200\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000000), 0xF0);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000200), 0x8);
	iounmap(dump_reg0);

	/* CSC reg */
	dump_reg0 = ioremap_nocache(0x0D0D3000, 0x214);
	if (dump_reg0 == NULL) {
		CCCI_MEM_LOG_TAG(md_index, TAG,
			"Dump MD failed to ioremap 0x214 bytes from 0x0D0D3000\n");
		return;
	}
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD CSC: 0x0D0D3000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000000), 0x214);
	iounmap(dump_reg0);

	/* ELM reg */
	dump_reg0 = ioremap_nocache(0x20350000, 0x52C);
	if (dump_reg0 == NULL) {
		CCCI_MEM_LOG_TAG(md_index, TAG,
			"Dump MD failed to ioremap 0x52C bytes from 0x20350000\n");
		return;
	}
#if defined(__MD_DEBUG_DUMP__)
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD ELM: 0x20350000\n");
#endif
#if defined(__MD_DEBUG_DUMP__)
	/* This dump might cause bus hang so enable it only when needed */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000000), 0x52C);
#endif
	iounmap(dump_reg0);

	/* USIP */
	dump_reg0 = ioremap_nocache(0x0D0C4400, 0x3500);
	if (dump_reg0 == NULL) {
		CCCI_MEM_LOG_TAG(md_index, TAG,
			"Dump MD failed to ioremap 0x3500 bytes from 0x0D0C4400\n");
		return;
	}
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD USIP: [0]0x0D0C4400, [1]0x0D0C4610, [2]0x0D0C5400, [3]0x0D0C5610, [4]0x0D0C7800\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000000), 0x100);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00000210), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00001000), 0x100);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00001210), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00003400), 0x100);
	/* [Pre-Action] config usip bus dbg sel 8 */
	mdreg_write32(MD_REG_USIP, 0x20001F);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00003400), 0xA0);
	/* [Pre-Action] config usip bus dbg sel 9 */
	mdreg_write32(MD_REG_USIP, 0x24001F);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00003400), 0xA0);
	/* [Pre-Action] config usip bus dbg sel 10 */
	mdreg_write32(MD_REG_USIP, 0x28001F);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00003400), 0xA0);
	/* [Pre-Action] config usip bus dbg sel 11 */
	mdreg_write32(MD_REG_USIP, 0x2C001F);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x00003400), 0xA0);
	iounmap(dump_reg0);
}

void md_dump_register_6779(unsigned int md_index)
{

	internal_md_dump_debug_register(md_index);
}
