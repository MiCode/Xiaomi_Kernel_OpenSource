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
/*! \file
*    \brief  Declaration of library functions
*
*    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/

#include <linux/memblock.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/random.h>

#include <connectivity_build_in_adapter.h>

#include "consys_reg_mng.h"
#include "mt6885_consys_reg.h"
#include "mt6885_consys_reg_offset.h"
#include "consys_hw.h"
#include "consys_reg_util.h"

#define LOG_TMP_BUF_SZ 256
#define CONSYS_POWER_MODE_LEGACY "Legacy"
#define CONSYS_POWER_MODE_RC "RC"

static int consys_reg_init(struct platform_device *pdev);
static int consys_reg_deinit(void);
static int consys_check_reg_readable(void);
static int consys_is_consys_reg(unsigned int addr);
static int consys_is_bus_hang(void);
static int consys_dump_bus_status(void);
static int consys_dump_conninfra_status(void);
static int consys_dump_cpupcr(enum conn_dump_cpupcr_type, int times, unsigned long interval_us);
static int consys_is_host_csr(unsigned long addr);

struct consys_reg_mng_ops g_dev_consys_reg_ops = {
	.consys_reg_mng_init = consys_reg_init,
	.consys_reg_mng_deinit = consys_reg_deinit,

	.consys_reg_mng_check_reable = consys_check_reg_readable,
	.consys_reg_mng_is_consys_reg = consys_is_consys_reg,
	.consys_reg_mng_is_bus_hang = consys_is_bus_hang,
	.consys_reg_mng_dump_bus_status = consys_dump_bus_status,
	.consys_reg_mng_dump_conninfra_status = consys_dump_conninfra_status,
	.consys_reg_mng_dump_cpupcr = consys_dump_cpupcr,
	.consys_reg_mng_is_host_csr = consys_is_host_csr
};


const char* consys_base_addr_index_to_str[CONSYS_BASE_ADDR_MAX] = {
	"CONN_INFRA_RGU_BASE",
	"CONN_INFRA_CFG_BASE",
	"CONN_HOST_CSR_TOP_BASE",
	"INFRACFG_AO_BASE",
	"TOPRGU_BASE",
	"SPM_BASE",
	"INFRACFG_BASE",
	"CONN_WT_SLP_CTL_REG",
	"CONN_AFE_CTL_REG",
	"CONN_INFRA_SYSRAM",
	"GPIO",
	"CONN_RF_SPI_MST_REG",
	"CONN_SEMAPHORE",
	"CONN_TOP_THERM_CTL",
	"IOCFG_RT",
};

struct consys_base_addr conn_reg;

struct consys_reg_mng_ops* get_consys_reg_mng_ops(void)
{
	return &g_dev_consys_reg_ops;
}

struct consys_base_addr* get_conn_reg_base_addr()
{
	return &conn_reg;
}

static void consys_bus_hang_dump_c(void)
{
	unsigned long debug_addr, out_addr;
	unsigned long value;
	int i;
	unsigned long debug_setting[] = {
		0xf0001, 0xe0001, 0xd0001, 0xc0001, 0xb0001, 0xa0001,
		0x90001, 0x80001, 0x70001, 0x60001, 0x50001, 0x40001,
		0x30001, 0x20001, 0x10001, 0x30002, 0x20002, 0x10002,
		0x40003, 0x30003, 0x20003, 0x10003
	};

	/* CONN2AP GALS RX status
	 * 0x1020_E804
	 *
	 * CONNINFRA sleep protect
	 * 0x1000_6158[2] ap2conn gals rx slp prot
	 * 0x1000_6150[13] ap2conn gals tx slp prot
	 *
	 * conninfra on2off off2on slp prot
	 * 0x1806_0184[5] conn_infra on2off slp prot
	 * 0x1806_0184[3] conn_infra off2on slp prot
	 */
	pr_info("[CONN_BUS_C] [%x][%x][%x] [%x]",
		CONSYS_REG_READ(CON_REG_INFRACFG_BASE_ADDR + INFRA_CONN2AP_GLAS_RC_ST),
		CONSYS_REG_READ(CON_REG_SPM_BASE_ADDR + SPM_BUS_PROTECT2_RDY),
		CONSYS_REG_READ(CON_REG_SPM_BASE_ADDR + SPM_BUS_PROTECT_RDY),
		CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_CONN_SLP_PROT_CTRL)
		);

	debug_addr = CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_CONN_INFRA_DEBUG_AO_DEBUGSYS;
	out_addr = CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_CONN_INFRA_DEBUG_CTRL_AO2SYS_OUT;

	for (i = 0; i < ARRAY_SIZE(debug_setting); i++) {
		CONSYS_REG_WRITE(debug_addr, debug_setting[i]);
		value = CONSYS_REG_READ(out_addr);
		pr_info("[CONN_BUS_C] addr=0x%x value=0x%08x", debug_setting[i], value);
	}
}

static void consys_bus_hang_dump_a_rc(void)
{
	unsigned int i;
	char tmp[LOG_TMP_BUF_SZ] = {'\0'};
	char tmp_buf[LOG_TMP_BUF_SZ] = {'\0'};

	for (i = 0xE50; i <= 0xE94; i += 4) {
		if (snprintf(tmp, LOG_TMP_BUF_SZ, "[%x]",
			CONSYS_REG_READ(CON_REG_SPM_BASE_ADDR + i)) >= 0)
			strncat(tmp_buf, tmp, strlen(tmp));
	}
	pr_info("[rc_trace] %s", tmp_buf);

	memset(tmp_buf, '\0', LOG_TMP_BUF_SZ);
	for (i = 0xE98; i <= 0xED4; i += 4) {
		if (snprintf(tmp, LOG_TMP_BUF_SZ, "[%x]",
			CONSYS_REG_READ(CON_REG_SPM_BASE_ADDR + i)) >= 0)
			strncat(tmp_buf, tmp, strlen(tmp));
	}
	pr_info("[rc_timer] %s", tmp_buf);
}

static void consys_bus_hang_dump_a(void)
{
	void __iomem *addr = NULL;
	unsigned int r1, r2, r3, r4, r5, r6, r7 = 0, r8 = 0, r9 = 0, r10, r11;
	char tmp_buf[LOG_TMP_BUF_SZ] = {'\0'};
	char rc_buf[LOG_TMP_BUF_SZ] = {'\0'};

	/*
	 * r1 : 0x1000_6110
	 * r2 : 0x1000_6114
	 */
	if (snprintf(tmp_buf, LOG_TMP_BUF_SZ, "[%s] [0x%x][0x%x]",
		(conn_hw_env.is_rc_mode ? CONSYS_POWER_MODE_RC : CONSYS_POWER_MODE_LEGACY),
		CONSYS_REG_READ(CON_REG_SPM_BASE_ADDR + SPM_PCM_REG13_DATA),
		CONSYS_REG_READ(CON_REG_SPM_BASE_ADDR + SPM_SRC_REQ_STA_0)) < 0)
		pr_warn("[%s] sprintf error\n", __func__);

	/* RC REQ STA
	 * r3 : 0x1000_6E28
	 * r4 : 0x1000_6E2C
	 * r5 : 0x1000_6E30
	 * r6 : 0x1000_6E34
	 */
	if (snprintf(rc_buf, LOG_TMP_BUF_SZ, "[%x][%x][%x][%x]",
			CONSYS_REG_READ(CON_REG_SPM_BASE_ADDR + SPM_RC_RC_M04_REQ_STA_0),
			CONSYS_REG_READ(CON_REG_SPM_BASE_ADDR + SPM_RC_RC_M05_REQ_STA_0),
			CONSYS_REG_READ(CON_REG_SPM_BASE_ADDR + SPM_RC_RC_M06_REQ_STA_0),
			CONSYS_REG_READ(CON_REG_SPM_BASE_ADDR + SPM_RC_RC_M07_REQ_STA_0)) < 0)
		pr_warn("[%s] sprintf error\n", __func__);

	/*
	 * 0x1000684C [28] DEBUG_IDX_VTCXO_STATE
	 * 0x1000684C [29] DEBUG_IDX_INFRA_STATE
	 * 0x1000684C [30] DEBUG_IDX_VRF18_STATE
	 * 0x1000684C [31] DEBUG_IDX_APSRC_STATE
	 * r7: 0x1000684C
	 */
	r1 = CONSYS_REG_READ(CON_REG_SPM_BASE_ADDR + SPM_PCM_WDT_LATCH_SPARE_0);

	/*
	 *  0x10006100[0] sc_26m_ck_off 	1'b0: 26M on; 1'b1
	 *  0x10006100[3] sc_axi_ck_off 	1'b0: bus ck on; 1'b1 bus ck off
	 *  0x10006100[5] sc_md26m_ck_off 	1'b0: MD 26M on; 1'b1 MD 26M off
	 *  0x10006100[20] sc_cksq0_off 	1'b0: clock square0 on; 1'b1 clock square off
	 * r8:0x10006100
	 */
	r2 = CONSYS_REG_READ(CON_REG_SPM_BASE_ADDR + SPM_PCM_REG7_DATA);

	/*
	 *  0x1000_6304[2] : pwr_on
	 *  0x1000_616c[1] : pwr_ack
	 *  0x1000_6304[3] : pwr_on_s
	 *  0x1000_6170[1] : pwr_ack_s
	 *  0x1000_6304[1] : iso_en
	 *  0x1000_6304[0] : ap_sw_rst
	 * r9  : 0x1000_616C
	 * r10 : 0x1000_6170
	 * r11 : 0x1000_6304
	 */
	r3 = CONSYS_REG_READ(CON_REG_SPM_BASE_ADDR + SPM_PWR_STATUS);
	r4 = CONSYS_REG_READ(CON_REG_SPM_BASE_ADDR + SPM_PWR_STATUS_2ND);
	r5 = CONSYS_REG_READ(CON_REG_SPM_BASE_ADDR + SPM_CONN_PWR_CON);

	/*
	 * sc_md_32k_ck_off
	 * r12 : 0x1000_644C
	 */
	r6 = CONSYS_REG_READ(CON_REG_SPM_BASE_ADDR + SPM_PLL_CON);

	/*
	 *  infra bus clock
	 *  r13 : 0x1000_0000 pdn_conn_32k
	 *  r14 : 0x1000_0010[2:0]
	 *    0: tck_26m_mx9_ck => 26M
	 *    1: mainpll_d4_d4 => 136.5M
	 *    2: mainpll_d7_d2 => 156M
	 *    3: mainpll_d4_d2 => 273M
	 *    4: mainpll_d5_d2 => 218.4M
	 *    5: mainpll_d6_d2 => 182M
	 *    6: osc_d4 => 65M
	 */
	addr = ioremap_nocache(0x10000000, 0x20);
	if (addr != NULL) {
		r7 = CONSYS_REG_READ(addr);
		r8 = CONSYS_REG_READ(addr + 0x10);
		iounmap(addr);
	}

	/*
	 *  r15 : 0x1000_0200 sc_md2_32k_off_en
	 */
	addr = ioremap_nocache(0x10000200, 0x20);
	if (addr != NULL) {
		r9 = CONSYS_REG_READ(addr);
		iounmap(addr);
	}

	/* ap2conn gals sleep protect status
	 *	- 0x1000_1724 [2] / 0x1000_1228 [13] (infracfg_ao)(rx/tx) (sleep protect enable ready)
	 *  r16 : 0x1000_1724
	 *  r17 : 0x1000_1228
	 */
	r10 = CONSYS_REG_READ(CON_REG_INFRACFG_AO_ADDR + INFRA_TOPAXI_PROTECTEN2_STA1_OFFSET);
	r11 = CONSYS_REG_READ(CON_REG_INFRACFG_AO_ADDR + INFRA_TOPAXI_PROTECTEN_STA1_OFFSET);

	pr_info("[CONN_BUS_A] %s %s [%x][%x][%x][%x][%x][%x] [%x][%x] [%x][%x][%x]", tmp_buf,
				rc_buf, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11);

	consys_bus_hang_dump_a_rc();
}

static void consys_bus_hang_dump_b(void)
{
	char tmp_buf[LOG_TMP_BUF_SZ] = {'\0'};
	unsigned int r1, r2, r3, r4, r5, r6, r7;

	/* Legacy Mode */
	/*
	 * 0x180602c0
	 * 			[4]: conn_srcclkena_ack
	 * 			[5]: conn_ap_bus_ack
	 * 			[6]: conn_apsrc_ack
	 * 			[7]: conn_ddr_en_ack
	 * cr1 : 0x1806_02c0
	 */
	if (snprintf(tmp_buf, LOG_TMP_BUF_SZ, "[%s] [%x]",
			(conn_hw_env.is_rc_mode ? CONSYS_POWER_MODE_RC : CONSYS_POWER_MODE_LEGACY),
			CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_DBG_DUMMY_0)) < 0)
		pr_warn("[%s] sprintf error\n", __func__);

	/* RC Mode */
	/*
	 * debug sel 0x18001B00[2:0] 3'b010
	 * 0x1806_02C8
	 *			[12] conn_srcclkena_0_bblpm_ack
	 *			[13] conn_srcclkena_0_fpm_ack
	 *			[28] conn_srcclkena_1_bblpm_ack
	 *			[29] conn_srcclkena_1_fpm_ack
	 * cr2 : 0x1806_02c8
	 */
	r1 = CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_DBG_DUMMY_2);

	/*
	 * Consys status check
	 * debug sel 0x18001B00[2:0] 3'b010
	 * 0x1806_02C8
	 *			[25] conninfra_bus_osc_en
	 *			[24] conn_pta_osc_en
	 *			[23] conn_thm_osc_en
	 *			[22] ap2conn_osc_en
	 *			[21] wfsys_osc_en
	 *			[20] bgfsys_osc_en
	 *			[19] gpssys_osc_en
	 *			[18] fmsys_osc_en
	 */
	/*CONSYS_REG_WRITE_MASK(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_DBG_MUX_SEL,
								0x2, 0x7);*/
	/*r1 = CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_DBG_DUMMY_2);*/

	/* Conninfra Off power state
	 * 0x1806_02CC
	 *			[15] power_enable
	 *			[14] power_on
	 *			[13] pwer_ack
	 *			[12] pwr_on_s
	 *			[11] pwr_ack_s
	 *			[10] iso_en
	 *			[9] conn_infra_off_xreset_b
	 *			[8] conn_infra_off_hreset_b
	 *
	 * cr3 : 0x1806_02CC
	 */
	r2 = CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_DBG_DUMMY_3);


	/* conn_infra_on clock 0x1020E504[0] = 1’b1
	 *
	 * cr4 : 0x1020_E504
	 */
	r3 = CONSYS_REG_READ(CON_REG_INFRACFG_BASE_ADDR +
			INFRA_AP2MD_GALS_CTL);

	/* Check conn_infra off bus clock
	 *	- write 0x1 to 0x1806_0000[0], reset clock detect
	 *	- 0x1806_0000[2]  conn_infra off bus clock (should be 1'b1 if clock exist)
	 *	- 0x1806_0000[1]  osc clock (should be 1'b1 if clock exist)
	 *
	 * cr5 : 0x1806_0000
	 */
	CONSYS_SET_BIT(CON_REG_HOST_CSR_ADDR, 0x1);
	udelay(200);
	r4 = CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR);

	/* conn_infra on2off sleep protect status
	 *	- 0x1806_0184[5] (sleep protect enable ready), should be 1'b0
	 * cr6 : 0x1806_0184
	 */
	r5 = CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_CONN_SLP_PROT_CTRL);

	/* CONN_HOST_CSR_DBG_DUMMY_4
	 * cr7 : 0x1806_02D0
	 */
	r6 = CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_DBG_DUMMY_4);

	/* 0x1806_02D4: dump bus timeout irq status
	 * cr8 : 0x1806_02D4
	 */
	r7 = CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_BUS_TIMEOUT_IRQ);

	pr_info("[CONN_BUS_B] infra_off %s [%x][%x] [%x][%x][%x] [%x] [%x]"
				, tmp_buf, r1, r2, r3, r4, r5, r6, r7);
	pr_info("[CONN_BUS_B] 0x1806_0294:[0x%08x] 0x1806_0220:[0x%08x]\n",
		CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + 0x0294),
		CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + 0x0220));
}

#if 0
static void consys_bus_hang_dump_d(void)
{
	int r1;

	/* check if consys off on
	 *	0x1806_02cc[8] 1'b1 ==> pwr on, 1'b0 ==> pwr off
	 */
	r1 = CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_DBG_DUMMY_3);
	if (((r1 >> 8) & 0x01) == 0) {
		pr_info("[CONN_BUS_D] conninfra off is not on");
		return;
	}

	/* PLL & OSC status
	 *  0x18001810[1] bpll_rdy
	 *  0x18001810[0] wpll_rdy
	 *
	 *  OSC
	 *  0x1800_180C
	 *			[3] : bus_bpll_sw_rdy, 1'b1 conninfra bus clock=166.4M
	 *			[2] : bus_bpll_sw_rdy, 1'b1 conninfra bus clock=83.2M
	 *			[1] : bus_bpll_sw_rdy, 1'b1 conninfra bus clock=32M
	 *			[0] : bus_bpll_sw_rdy, 1'b1 conninfra bus clock=osc
	 *
	 * 0x1800_1160[22] ap2conn gals rx slp prot
	 * 0x1800_1630[3] conn2ap gals tx slp prot
	 * 0x1800_1634[3] conn2ap gals rx slp prot
	 *
	 * 0x1800_1628[3] conn_infra on2off slp pro
	 * 0x1800_162c[3] conn_infra off2on slp pro
	 *
	 * 0x1800_161C[3] gps2conn gals tx slp prot
	 * 0x1800_161C[7] gps2conn gals rx slp prot
	 * 0x1800_1618[7] conn2gps gals rx slp prot
	 * 0x1800_1618[3] conn2gps gals tx slp prot
	 *
	 * 0x1800_1614[3] bt2conn gals tx slp prot
	 * 0x1800_1614[7] bt2conn gals rx slp prot
	 * 0x1800_1610[3] conn2bt gals tx slp prot
	 * 0x1800_1610[7] conn2bt gals rx slp prot
	 *
	 * 0x1800_1620[3] wf2conn slp prot
	 */
	pr_info("[CONN_BUS_D] [%x][%x] [%x][%x][%x] [%x][%x] [%x][%x] [%x][%x] [%x]",
		CONSYS_REG_READ(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_PLL_STATUS), /* PLL */
		CONSYS_REG_READ(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_OSC_STATUS), /* OSC */

		CONSYS_REG_READ(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_GALS_AP2CONN_GALS_DBG),
		CONSYS_REG_READ(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_GALS_CONN2AP_TX_SLP_CTRL),
		CONSYS_REG_READ(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_GALS_CONN2AP_RX_SLP_CTRL),

		CONSYS_REG_READ(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_ON_BUS_SLP_CTRL), /* on2off */
		CONSYS_REG_READ(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_OFF_BUS_SLP_CTRL),/* off2on */

		CONSYS_REG_READ(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_GALS_GPS2CONN_SLP_CTRL), /* gps2conn */
		CONSYS_REG_READ(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_GALS_CONN2GPS_SLP_CTRL), /* conn2gps */

		CONSYS_REG_READ(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_GALS_BT2CONN_SLP_CTRL), /* bt2conn */
		CONSYS_REG_READ(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_GALS_CONN2BT_SLP_CTRL), /* conn2bt */

		CONSYS_REG_READ(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_WF_SLP_CTRL) /* w2conn */
		);
}
#endif

static int consys_dump_bus_status(void)
{
	consys_bus_hang_dump_c();
	return 0;
}

static int consys_dump_conninfra_status(void)
{
	consys_bus_hang_dump_a();
	consys_bus_hang_dump_b();
	return 0;
}

int consys_dump_cpupcr(enum conn_dump_cpupcr_type type, int times, unsigned long interval_us)
{
	int i;

	for (i = 0; i < times; i++) {
		pr_info("%s: wm pc=0x%x, wm lp=0x%x, bt pc=0x%x",
			__func__,
			CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_WM_MCU_PC_DBG),
			CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_WM_MCU_GPR_DBG),
			CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_BGF_MCU_PC_DBG));
		if (interval_us > 1000)
			usleep_range(interval_us - 100, interval_us + 100);
		else
			udelay(interval_us);
	}

	return 0;
}

static int consys_is_bus_hang(void)
{
	int r;

	/* dump SPM register */
	consys_bus_hang_dump_a();

	/* STEP - 1 */
	/*
	 * check kernel API lastbus_timeout_dump
	 *   if return > 0 -> return 0x1
	 */

	/* 1. Check ap2conn gals sleep protect status
	 *	- 0x1000_1724 [2] / 0x1000_1228 [13] (infracfg_ao)(rx/tx) (sleep protect enable ready)
	 * 		both of them should be 1'b0  (CR at ap side)
	 */
	r = CONSYS_REG_READ_BIT(CON_REG_INFRACFG_AO_ADDR +
			INFRA_TOPAXI_PROTECTEN2_STA1_OFFSET, (0x1 << 2));
	if (r != 0)
		return CONNINFRA_AP2CONN_RX_SLP_PROT_ERR;

	r = CONSYS_REG_READ_BIT(CON_REG_INFRACFG_AO_ADDR +
			INFRA_TOPAXI_PROTECTEN_STA1_OFFSET, (0x1 << 13));
	if (r != 0)
		return CONNINFRA_AP2CONN_TX_SLP_PROT_ERR;

	/* 2. Check conn_infra_on clock 0x1020E504[0] = 1’b1 */
	r = CONSYS_REG_READ_BIT(CON_REG_INFRACFG_BASE_ADDR +
			INFRA_AP2MD_GALS_CTL, 0x1);
	if (r != 1)
		return CONNINFRA_AP2CONN_CLK_ERR;

	/* STEP - 2 */
	consys_bus_hang_dump_b();

#if 0
	/* 3. Check conn_infra off bus clock
	 *	- write 0x1 to 0x1806_0000[0], reset clock detect
	 *	- 0x1806_0000[2]  conn_infra off bus clock (should be 1'b1 if clock exist)
	 *	- 0x1806_0000[1]  osc clock (should be 1'b1 if clock exist)
	 */
	CONSYS_SET_BIT(CON_REG_HOST_CSR_ADDR, 0x1);
	udelay(500);
	r = CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR);
	if ((r & TOP_BUS_MUC_STAT_HCLK_FR_CK_DETECT_BIT) == 0 ||
		(r & TOP_BUS_MUC_STAT_OSC_CLK_DETECT_BIT) == 0)
		ret |= CONNINFRA_INFRA_BUS_CLK_ERR;
#endif

	/* 4. Check conn_infra off domain bus hang irq status
	 *	- 0x1806_02d4[0], should be 1'b1, or means conn_infra off bus might hang
	 */
	r = CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_BUS_TIMEOUT_IRQ);

	if ((r & 0x1) != 0x1) {
		pr_err("conninfra off bus might hang cirq=[0x%08x]", r);
		consys_bus_hang_dump_c();
		return CONNINFRA_INFRA_BUS_HANG_IRQ;
	}
	consys_bus_hang_dump_c();

#if 0
	/* 5. Check conn_infra on2off sleep protect status
	 *	- 0x1806_0184[5] (sleep protect enable ready), should be 1'b0
	 */
	r = CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_CONN_SLP_PROT_CTRL);
	if (r & TOP_SLP_PROT_CTRL_CONN_INFRA_ON2OFF_SLP_PROT_ACK_BIT)
		ret |= CONNINFRA_ON2OFF_SLP_PROT_ERR;
	if (ret) {
		pr_info("[%s] ret=[%x]", __func__, ret);
		consys_bus_hang_dump_c();
		return ret;
	}
#endif
	/*consys_bus_hang_dump_d();*/

	return 0;

}

int consys_check_reg_readable(void)
{
	int r;
	unsigned int rnd;
	int delay_time = 1, spent = delay_time, max_wait = 16000; // 1.6 ms
	int retry = max_wait / 10;

	/* STEP - 1 */
	/*
	 * check kernel API lastbus_timeout_dump
	 *   if return > 0 -> return 0x1
	 */


	/* 1. Check ap2conn gals sleep protect status
	 *	- 0x1000_1724 [2] / 0x1000_1228 [13] (infracfg_ao)(rx/tx) (sleep protect enable ready)
	 * 		both of them should be 1'b0  (CR at ap side)
	 */
	r = CONSYS_REG_READ_BIT(CON_REG_INFRACFG_AO_ADDR +
			INFRA_TOPAXI_PROTECTEN2_STA1_OFFSET, (0x1 << 2));
	if (r != 0)
		return 0;

	r = CONSYS_REG_READ_BIT(CON_REG_INFRACFG_AO_ADDR +
			INFRA_TOPAXI_PROTECTEN_STA1_OFFSET, (0x1 << 13));
	if (r != 0)
		return 0;

	/* STEP - 2 */

	/* 2. Check conn_infra_on clock 0x1020E504[0] = 1’b1 */
	r = CONSYS_REG_READ_BIT(CON_REG_INFRACFG_BASE_ADDR +
			INFRA_AP2MD_GALS_CTL, 0x1);
	if (r != 1)
		return 0;

	/* 3. Check conn_infra off bus clock
	 *	- write 0x1 to 0x1806_0000[0], reset clock detect
	 *	- 0x1806_0000[1]  conn_infra off bus clock (should be 1'b1 if clock exist)
	 *	- 0x1806_0000[2]  osc clock (should be 1'b1 if clock exist)
	 */

	while (retry > 0 && spent < max_wait) {
		CONSYS_SET_BIT(CON_REG_HOST_CSR_ADDR, 0x1);
		udelay(delay_time);
		r = CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR);
		if ((r & TOP_BUS_MUC_STAT_HCLK_FR_CK_DETECT_BIT) == 0 ||
			(r & TOP_BUS_MUC_STAT_OSC_CLK_DETECT_BIT) == 0) {
			spent += delay_time;
			retry--;
			if (retry == 0)
				pr_info("[%s] retry=0 r=[%x]", __func__, r);
			else
				delay_time = 10;
			rnd = get_random_int() % 10;
			spent += rnd;
			udelay(rnd);
			continue;
		}
		break;
	}
	if (retry == 0 || spent >= max_wait) {
		pr_info("[%s] readable fail = bus clock retry=[%d] spent=[%d]", __func__,
					retry, spent);
		return 0;
	}

	/* 4. Check conn_infra off domain bus hang irq status
	 *	- 0x1806_02d4[0], should be 1'b1, or means conn_infra off bus might hang
	 */
	r = CONSYS_REG_READ_BIT(CON_REG_HOST_CSR_ADDR +
			CONN_HOST_CSR_TOP_BUS_TIMEOUT_IRQ, 0x1);
	if (r != 0x1)
		return 0;

	/* 5. Check conn_infra on2off sleep protect status
	 *	- 0x1806_0184[5] (sleep protect enable ready), should be 1'b0
	 */
	r = CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_CONN_SLP_PROT_CTRL);
	if (r & TOP_SLP_PROT_CTRL_CONN_INFRA_ON2OFF_SLP_PROT_ACK_BIT)
		return 0;

	return 1;
}


int consys_is_consys_reg(unsigned int addr)
{
	return 0;
}


static int consys_is_host_csr(unsigned long addr)
{
	struct consys_reg_base_addr *host_csr_addr =
			&conn_reg.reg_base_addr[CONN_HOST_CSR_TOP_BASE_INDEX];

	if (addr >= host_csr_addr->phy_addr &&
			addr < host_csr_addr->phy_addr + host_csr_addr->size)
		return 1;
	return 0;
}

unsigned long consys_reg_validate_idx_n_offset(unsigned int idx, unsigned long offset)
{
	unsigned long res;

	if (idx < 0 || idx >= CONSYS_BASE_ADDR_MAX) {
		pr_warn("ConsysReg failed: No support the base %d\n", idx);
		return 0;
	}

	res = conn_reg.reg_base_addr[idx].phy_addr;

	if (res == 0) {
		pr_warn("ConsysReg failed: No support the base idx is 0 index=[%d]\n", idx);
		return 0;
	}

	if (offset >= conn_reg.reg_base_addr[idx].size) {
		pr_warn("ConnReg failed: index(%d), offset(%d) over max size(%llu) %s\n",
				idx, (int) offset, conn_reg.reg_base_addr[idx].size);
		return 0;
	}
	return res;
}

int consys_find_can_write_reg(unsigned int *idx, unsigned long *offset)
{
	int i;
	size_t addr = 0, addr_offset;
	int max, mask = 0x0000000F;
	int before, after, ret;

	addr = conn_reg.reg_base_addr[CONN_INFRA_RGU_BASE_INDEX].vir_addr;
	max = conn_reg.reg_base_addr[CONN_INFRA_RGU_BASE_INDEX].size;

	pr_info("[%s] addr=[%p]\n", __func__, addr);

	for (i = 0x0; i < max; i += 0x4) {
		ret = 0;
		addr_offset = addr + i;
		before = CONSYS_REG_READ(addr_offset);
		CONSYS_REG_WRITE_MASK(addr_offset, 0xFFFFFFFF, mask);
		after = CONSYS_REG_READ(addr_offset);
		if ((after & mask) != (0xFFFFFFFF & mask))
			ret = -1;

		CONSYS_REG_WRITE_MASK(addr_offset, 0x0, mask);
		after = CONSYS_REG_READ(addr_offset);
		if ((after & mask) != (0x0 & mask))
			ret = -1;

		CONSYS_REG_WRITE_MASK(addr_offset, before, mask);

		pr_info("[%s] addr=[%p] [%d]\n", __func__, addr_offset, ret);
		if (ret == 0) {
			*idx = CONN_INFRA_RGU_BASE_INDEX;
			*offset = i;
			return 0;
		}
	}
	return -1;

}


unsigned long consys_reg_get_phy_addr_by_idx(unsigned int idx)
{
	if (idx >= CONSYS_BASE_ADDR_MAX)
		return 0;
	return conn_reg.reg_base_addr[idx].phy_addr;
}

unsigned long consys_reg_get_virt_addr_by_idx(unsigned int idx)
{
	if (idx >= CONSYS_BASE_ADDR_MAX)
		return 0;
	return conn_reg.reg_base_addr[idx].vir_addr;
}


int consys_reg_get_chip_id_idx_offset(unsigned int *idx, unsigned long *offset)
{
	*idx = CONN_INFRA_CFG_BASE_INDEX;
	*offset = CONN_CFG_ID_OFFSET;
	return 0;
}

int consys_reg_get_reg_symbol_num(void)
{
	return CONSYS_BASE_ADDR_MAX;
}


int consys_reg_init(struct platform_device *pdev)
{
	int ret = -1;
	struct device_node *node = NULL;
	struct consys_reg_base_addr *base_addr = NULL;
	struct resource res;
	int flag, i = 0;

	node = pdev->dev.of_node;
	pr_info("[%s] node=[%p]\n", __func__, node);
	if (node) {
		for (i = 0; i < CONSYS_BASE_ADDR_MAX; i++) {
			base_addr = &conn_reg.reg_base_addr[i];

			ret = of_address_to_resource(node, i, &res);
			if (ret) {
				pr_err("Get Reg Index(%d-%s) failed",
						i, consys_base_addr_index_to_str[i]);
				continue;
			}

			base_addr->phy_addr = res.start;
			base_addr->vir_addr =
				(unsigned long) of_iomap(node, i);
			of_get_address(node, i, &(base_addr->size), &flag);

			pr_info("Get Index(%d-%s) phy(0x%zx) baseAddr=(0x%zx) size=(0x%zx)",
				i, consys_base_addr_index_to_str[i], base_addr->phy_addr,
				base_addr->vir_addr, base_addr->size);
		}

	} else {
		pr_err("[%s] can't find CONSYS compatible node\n", __func__);
		return ret;
	}
	return 0;

}

static int consys_reg_deinit(void)
{
	int i = 0;

	for (i = 0; i < CONSYS_BASE_ADDR_MAX; i++) {
		if (conn_reg.reg_base_addr[i].vir_addr) {
			pr_info("[%d] Unmap %s (0x%zx)",
				i, consys_base_addr_index_to_str[i],
				conn_reg.reg_base_addr[i].vir_addr);
			iounmap((void __iomem*)conn_reg.reg_base_addr[i].vir_addr);
			conn_reg.reg_base_addr[i].vir_addr = 0;
		}
	}

	return 0;
}

