/*
 * Copyright (C) 2019 MediaTek Inc.
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
#include "gps_dl_config.h"

#include "gps_dl_context.h"
#include "gps_dl_hw_api.h"
#include "gps_dl_hw_dep_api.h"
#include "gps_dl_hw_priv_util.h"

#include "conn_infra/conn_host_csr_top.h"
#include "conn_infra/conn_infra_cfg.h"
#if GPS_DL_HAS_PTA
#include "conn_infra/conn_uart_pta.h"
#include "conn_infra/conn_pta6.h"
#endif
#include "conn_infra/conn_semaphore.h"
#include "conn_infra/conn_rf_spi_mst_reg.h"

void gps_dl_hw_set_gps_emi_remapping(unsigned int _20msb_of_36bit_phy_addr)
{
	GDL_HW_SET_CONN_INFRA_ENTRY(
		CONN_HOST_CSR_TOP_CONN2AP_REMAP_GPS_EMI_BASE_ADDR_CONN2AP_REMAP_GPS_EMI_BASE_ADDR,
		_20msb_of_36bit_phy_addr);
}

unsigned int gps_dl_hw_get_gps_emi_remapping(void)
{
	return GDL_HW_GET_CONN_INFRA_ENTRY(
		CONN_HOST_CSR_TOP_CONN2AP_REMAP_GPS_EMI_BASE_ADDR_CONN2AP_REMAP_GPS_EMI_BASE_ADDR);
}

void gps_dl_hw_print_hw_status(enum gps_dl_link_id_enum link_id, bool dump_rf_cr)
{
	struct gps_dl_hw_dma_status_struct a2d_dma_status, d2a_dma_status;
	struct gps_dl_hw_usrt_status_struct usrt_status;
	enum gps_dl_hal_dma_ch_index a2d_dma_ch, d2a_dma_ch;

	if (gps_dl_show_reg_wait_log())
		GDL_LOGXD(link_id, "");

	if (link_id == GPS_DATA_LINK_ID0) {
		a2d_dma_ch = GPS_DL_DMA_LINK0_A2D;
		d2a_dma_ch = GPS_DL_DMA_LINK0_D2A;
	} else if (link_id == GPS_DATA_LINK_ID1) {
		a2d_dma_ch = GPS_DL_DMA_LINK1_A2D;
		d2a_dma_ch = GPS_DL_DMA_LINK1_D2A;
	} else
		return;

	gps_dl_hw_save_usrt_status_struct(link_id, &usrt_status);
	gps_dl_hw_print_usrt_status_struct(link_id, &usrt_status);

	gps_dl_hw_save_dma_status_struct(a2d_dma_ch, &a2d_dma_status);
	gps_dl_hw_print_dma_status_struct(a2d_dma_ch, &a2d_dma_status);

	gps_dl_hw_save_dma_status_struct(d2a_dma_ch, &d2a_dma_status);
	gps_dl_hw_print_dma_status_struct(d2a_dma_ch, &d2a_dma_status);

	GDL_HW_RD_GPS_REG(0x80073160); /* DL0 */
	GDL_HW_RD_GPS_REG(0x80073134); /* DL1 */

	GDL_HW_RD_CONN_INFRA_REG(CONN_RF_SPI_MST_ADDR_SPI_STA_ADDR);
	GDL_HW_RD_CONN_INFRA_REG(CONN_RF_SPI_MST_ADDR_SPI_GPS_GPS_ADDR_ADDR);
	GDL_HW_RD_CONN_INFRA_REG(CONN_RF_SPI_MST_ADDR_SPI_GPS_GPS_WDAT_ADDR);
	GDL_HW_RD_CONN_INFRA_REG(CONN_RF_SPI_MST_ADDR_SPI_GPS_GPS_RDAT_ADDR);
	GDL_HW_RD_CONN_INFRA_REG(CONN_RF_SPI_MST_ADDR_SPI_STA_ADDR);

	if (dump_rf_cr) {
		gps_dl_hw_gps_dump_top_rf_cr();
		gps_dl_hw_gps_dump_gps_rf_cr();
	}

	/* only need for L1 */
	gps_each_link_set_bool_flag(GPS_DATA_LINK_ID0, LINK_NEED_A2Z_DUMP, true);
}

void gps_dl_hw_do_gps_a2z_dump(void)
{
	GDL_HW_WR_GPS_REG(0x80073120, 1); /* enable A2Z */
	GDL_HW_RD_GPS_REG(0x800706C0);
	GDL_HW_RD_GPS_REG(0x80070450);
	GDL_HW_RD_GPS_REG(0x80080450);
}

void gps_dl_hw_dump_sleep_prot_status(void)
{
	bool show_log = true;

	show_log = gps_dl_set_show_reg_rw_log(true);
	GDL_HW_DUMP_SLP_RPOT_STATUS();
	gps_dl_set_show_reg_rw_log(show_log);
}

void gps_dl_hw_dump_host_csr_gps_info(bool force_show_log)
{
	int i;
	bool show_log = true;

	if (force_show_log)
		show_log = gps_dl_set_show_reg_rw_log(true);

#if 0
	GDL_HW_GET_CONN_INFRA_ENTRY(CONN_HOST_CSR_TOP_HOST2GPS_DEGUG_SEL_HOST2GPS_DEGUG_SEL);
	GDL_HW_GET_CONN_INFRA_ENTRY(CONN_HOST_CSR_TOP_GPS_CFG2HOST_DEBUG_GPS_CFG2HOST_DEBUG);
#else
	gps_dl_bus_rd_opt(GPS_DL_CONN_INFRA_BUS,
		CONN_HOST_CSR_TOP_HOST2GPS_DEGUG_SEL_ADDR,
		BMASK_RW_FORCE_PRINT);
	gps_dl_bus_rd_opt(GPS_DL_CONN_INFRA_BUS,
		CONN_HOST_CSR_TOP_GPS_CFG2HOST_DEBUG_ADDR,
		BMASK_RW_FORCE_PRINT);
#endif

	for (i = 0xA2; i <= 0xB7; i++) {
#if 0
		GDL_HW_SET_CONN_INFRA_ENTRY(CONN_HOST_CSR_TOP_HOST2GPS_DEGUG_SEL_HOST2GPS_DEGUG_SEL, i);
		GDL_HW_GET_CONN_INFRA_ENTRY(CONN_HOST_CSR_TOP_GPS_CFG2HOST_DEBUG_GPS_CFG2HOST_DEBUG);
#else
		gps_dl_bus_wr_opt(GPS_DL_CONN_INFRA_BUS,
			CONN_HOST_CSR_TOP_HOST2GPS_DEGUG_SEL_ADDR, i,
			BMASK_RW_FORCE_PRINT);
		gps_dl_bus_rd_opt(GPS_DL_CONN_INFRA_BUS,
			CONN_HOST_CSR_TOP_GPS_CFG2HOST_DEBUG_ADDR,
			BMASK_RW_FORCE_PRINT);
#endif
	}

	if (force_show_log)
		gps_dl_set_show_reg_rw_log(show_log);
}

void gps_dl_bus_check_and_print(unsigned int host_addr)
{
	/* not do rw check because here is the checking */
	GDL_LOGI("for addr = 0x%08x", host_addr);
	gps_dl_bus_wr_opt(GPS_DL_CONN_INFRA_BUS,
		CONN_HOST_CSR_TOP_CONN_INFRA_DEBUG_AO_DEBUGSYS_ADDR, 0x000D0001,
		BMASK_RW_FORCE_PRINT);

	gps_dl_bus_rd_opt(GPS_DL_CONN_INFRA_BUS,
		CONN_HOST_CSR_TOP_CONN_INFRA_DEBUG_CTRL_AO2SYS_OUT_ADDR,
		BMASK_RW_FORCE_PRINT);

	gps_dl_bus_wr_opt(GPS_DL_CONN_INFRA_BUS,
		CONN_HOST_CSR_TOP_CONN_INFRA_DEBUG_AO_DEBUGSYS_ADDR, 0x000B0001,
		BMASK_RW_FORCE_PRINT);

	gps_dl_bus_rd_opt(GPS_DL_CONN_INFRA_BUS,
		CONN_HOST_CSR_TOP_CONN_INFRA_DEBUG_CTRL_AO2SYS_OUT_ADDR,
		BMASK_RW_FORCE_PRINT);

	gps_dl_bus_wr_opt(GPS_DL_CONN_INFRA_BUS,
		CONN_HOST_CSR_TOP_CONN_INFRA_DEBUG_AO_DEBUGSYS_ADDR, 0x000A0001,
		BMASK_RW_FORCE_PRINT);

	gps_dl_bus_rd_opt(GPS_DL_CONN_INFRA_BUS,
		CONN_HOST_CSR_TOP_CONN_INFRA_DEBUG_CTRL_AO2SYS_OUT_ADDR,
		BMASK_RW_FORCE_PRINT);
}

void gps_dl_hw_dump_host_csr_conninfra_info_inner(unsigned int selection, int n)
{
	int i;

	for (i = 0; i < n; i++) {
#if 0
		GDL_HW_SET_CONN_INFRA_ENTRY(
			CONN_HOST_CSR_TOP_CONN_INFRA_DEBUG_AO_DEBUGSYS_CONN_INFRA_DEBUG_CTRL_AO_DEBUGSYS_CTRL,
			selection);
		GDL_HW_GET_CONN_INFRA_ENTRY(
			CONN_HOST_CSR_TOP_CONN_INFRA_DEBUG_CTRL_AO2SYS_OUT_CONN_INFRA_DEBUG_CTRL_AO2SYS_OUT);
#else
		/* Due to RW_DO_CHECK might be enabled, not use
		 * GDL_HW_SET_CONN_INFRA_ENTRY and GDL_HW_GET_CONN_INFRA_ENTRY to avoid redundant print.
		 */
		gps_dl_bus_wr_opt(GPS_DL_CONN_INFRA_BUS,
			CONN_HOST_CSR_TOP_CONN_INFRA_DEBUG_AO_DEBUGSYS_ADDR, selection,
			BMASK_RW_FORCE_PRINT);

		gps_dl_bus_rd_opt(GPS_DL_CONN_INFRA_BUS,
			CONN_HOST_CSR_TOP_CONN_INFRA_DEBUG_CTRL_AO2SYS_OUT_ADDR,
			BMASK_RW_FORCE_PRINT);
#endif
		selection -= 0x10000;
	}
}

void gps_dl_hw_dump_host_csr_conninfra_info(bool force_show_log)
{
	bool show_log = true;

	if (force_show_log)
		show_log = gps_dl_set_show_reg_rw_log(true);

	gps_dl_hw_dump_host_csr_conninfra_info_inner(0x000F0001, 15);
	gps_dl_hw_dump_host_csr_conninfra_info_inner(0x00030002, 3);
	gps_dl_hw_dump_host_csr_conninfra_info_inner(0x00040003, 4);

	if (force_show_log)
		gps_dl_set_show_reg_rw_log(show_log);
}

#if GPS_DL_HAS_PTA
/* CONN_INFRA_CFG_CKGEN_BUS_ADDR[5:2] */
#define CONN_INFRA_CFG_PTA_CLK_ADDR CONN_INFRA_CFG_CKGEN_BUS_ADDR
#define CONN_INFRA_CFG_PTA_CLK_MASK (\
	CONN_INFRA_CFG_CKGEN_BUS_CONN_CO_EXT_UART_PTA_OSC_CKEN_MASK   | \
	CONN_INFRA_CFG_CKGEN_BUS_CONN_CO_EXT_UART_PTA_HCLK_CKEN_MASK  | \
	CONN_INFRA_CFG_CKGEN_BUS_CONN_CO_EXT_UART_PTA5_OSC_CKEN_MASK  | \
	CONN_INFRA_CFG_CKGEN_BUS_CONN_CO_EXT_UART_PTA5_HCLK_CKEN_MASK)
#define CONN_INFRA_CFG_PTA_CLK_SHFT \
	CONN_INFRA_CFG_CKGEN_BUS_CONN_CO_EXT_UART_PTA5_HCLK_CKEN_SHFT

bool gps_dl_hw_is_pta_clk_cfg_ready(void)
{
	unsigned int pta_clk_cfg;
	bool okay = true;

	pta_clk_cfg = GDL_HW_GET_CONN_INFRA_ENTRY(CONN_INFRA_CFG_PTA_CLK);
	if (pta_clk_cfg != 0xF) {
		/* clock cfg is default ready, no need to set it
		 * if not as excepted, skip pta and pta_uart init
		 */
		okay = false;
	}

	if (!okay)
		GDL_LOGE("pta_clk_cfg = 0x%x", pta_clk_cfg);
	return okay;
}

void gps_dl_hw_set_ptk_clk_cfg(void)
{
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_INFRA_CFG_PTA_CLK, 0xF);
}

bool gps_dl_hw_is_pta_uart_init_done(void)
{
	bool done;
	unsigned int pta_uart_en;

	pta_uart_en = GDL_HW_GET_CONN_INFRA_ENTRY(CONN_PTA6_RO_PTA_CTRL_ro_uart_apb_hw_en);
	done = (pta_uart_en == 1);
	GDL_LOGD("done = %d, pta_uart_en = %d", done, pta_uart_en);

	return done;
}

/* Only check bit7, so not use CONN_UART_PTA_FCR_RFTL */
#define CONN_UART_PTA_FCR_RFTL_HIGH_BIT_ADDR CONN_UART_PTA_FCR_ADDR
#define CONN_UART_PTA_FCR_RFTL_HIGH_BIT_MASK 0x00000080
#define CONN_UART_PTA_FCR_RFTL_HIGH_BIT_SHFT 7

bool gps_dl_hw_init_pta_uart(void)
{
#if 0
	unsigned int pta_uart_en;
#endif
	bool show_log;
	bool poll_okay;
	bool reg_rw_log = gps_dl_log_reg_rw_is_on(GPS_DL_REG_RW_HOST_CSR_PTA_INIT);

	/* 20191008 after DE checking, bellow steps are no need */
#if 0
	/* Set pta uart to MCU mode before init it.
	 * Note: both wfset and btset = 0, then pta_uart_en should become 0,
	 * set one of them = 1, then pta_uart_en should become 1.
	 */
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_PTA6_WFSET_PTA_CTRL_r_wfset_uart_apb_hw_en, 0);
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_PTA6_BTSET_PTA_CTRL_r_btset_uart_apb_hw_en, 0);
	pta_uart_en = GDL_HW_GET_CONN_INFRA_ENTRY(CONN_PTA6_RO_PTA_CTRL_ro_uart_apb_hw_en);

	if (pta_uart_en != 0) {
		GDL_LOGE("ro_uart_apb_hw_en not become 0, fail");
		return false;
	}
#endif

	if (reg_rw_log) {
		gps_dl_hw_dump_host_csr_conninfra_info(true);
		show_log = gps_dl_set_show_reg_rw_log(true);
	}
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_UART_PTA_HIGHSPEED_SPEED, 3);
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_UART_PTA_SAMPLE_COUNT_SAMPLE_COUNT, 5);
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_UART_PTA_SAMPLE_POINT_SAMPLE_POINT, 2);

	/* UART_PTA_BASE + 0x3C = 0x12, this step is no-need now
	 * GDL_HW_SET_CONN_INFRA_ENTRY(CONN_UART_PTA_GUARD_GUARD_CNT, 2);
	 * GDL_HW_SET_CONN_INFRA_ENTRY(CONN_UART_PTA_GUARD_GUARD_EN, 1);
	 */

	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_UART_PTA_VFIFO_EN_RX_TIME_EN, 1); /* bit7 */
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_UART_PTA_VFIFO_EN_PTA_RX_FE_EN, 1); /* bit3 */
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_UART_PTA_VFIFO_EN_PTA_RX_MODE, 1); /* bit2 */

	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_UART_PTA_FRACDIV_L_FRACDIV_L, 0x55);
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_UART_PTA_FRACDIV_M_FRACDIV_M, 2);

	/* UART_PTA_BASE + 0x3C[4] = 0, this step is no-need now
	 * GDL_HW_SET_CONN_INFRA_ENTRY(CONN_UART_PTA_GUARD_GUARD_EN, 0);
	 */

	GDL_HW_WR_CONN_INFRA_REG(CONN_UART_PTA_LCR_ADDR, 0xBF);
	GDL_HW_WR_CONN_INFRA_REG(CONN_UART_PTA_DLL_ADDR, 1);
	GDL_HW_WR_CONN_INFRA_REG(CONN_UART_PTA_DLM_ADDR, 0);
	GDL_HW_WR_CONN_INFRA_REG(CONN_UART_PTA_LCR_ADDR, 3);

	/* 20191008 after DE checking, add CONN_UART_PTA_FCR_ADDR read-back checking */

	/* dump value before setting */
	GDL_HW_RD_CONN_INFRA_REG(CONN_UART_PTA_FCR_ADDR);
	GDL_HW_WR_CONN_INFRA_REG(CONN_UART_PTA_FCR_ADDR, 0x37);
	/* dump value after setting and poll until bit7 become 1 or timeout */
	GDL_HW_POLL_CONN_INFRA_ENTRY(CONN_UART_PTA_FCR_RFTL_HIGH_BIT, 1, POLL_DEFAULT, &poll_okay);
	if (!poll_okay) {
		if (reg_rw_log)
			gps_dl_set_show_reg_rw_log(show_log);
		GDL_LOGE("CONN_UART_PTA_FCR bit7 not become 1, fail");
		return false;
	}

	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_PTA6_WFSET_PTA_CTRL_r_wfset_uart_apb_hw_en, 1);

	/* 20191008 after DE checking, add this step */
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_PTA6_BTSET_PTA_CTRL_r_btset_uart_apb_hw_en, 1);

	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_PTA6_WFSET_PTA_CTRL_r_wfset_lte_pta_en, 1);

	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_PTA6_TMR_CTRL_1_r_idc_2nd_byte_tmout, 4); /* us */

	if (reg_rw_log)
		gps_dl_set_show_reg_rw_log(show_log);

	return true;
}

void gps_dl_hw_deinit_pta_uart(void)
{
	/* Currently no need to do pta uart deinit */
}


#define PTA_1M_CNT_VALUE 26 /* mobile platform uses 26M */

bool gps_dl_hw_is_pta_init_done(void)
{
	bool done;
	unsigned int pta_en;
	unsigned int pta_arb_en;
	unsigned int pta_1m_cnt;

	pta_en = GDL_HW_GET_CONN_INFRA_ENTRY(CONN_PTA6_RO_PTA_CTRL_ro_pta_en);
	pta_arb_en = GDL_HW_GET_CONN_INFRA_ENTRY(CONN_PTA6_RO_PTA_CTRL_ro_en_pta_arb);
	pta_1m_cnt = GDL_HW_GET_CONN_INFRA_ENTRY(CONN_PTA6_PTA_CLK_CFG_r_pta_1m_cnt);

	done = ((pta_en == 1) && (pta_arb_en == 1) && (pta_1m_cnt == PTA_1M_CNT_VALUE));
	GDL_LOGD("done = %d, pta_en = %d, pta_arb_en = %d, pta_1m_cnt = 0x%x",
		done, pta_en, pta_arb_en, pta_1m_cnt);

	return done;
}

void gps_dl_hw_init_pta(void)
{
	unsigned int pta_en;
	unsigned int pta_arb_en;

	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_PTA6_PTA_CLK_CFG_r_pta_1m_cnt, PTA_1M_CNT_VALUE);

	/* Note: GPS use WFSET */
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_PTA6_WFSET_PTA_CTRL_r_wfset_en_pta_arb, 1);
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_PTA6_WFSET_PTA_CTRL_r_wfset_pta_en, 1);

	/* just confirm status change properly */
	pta_en = GDL_HW_GET_CONN_INFRA_ENTRY(CONN_PTA6_RO_PTA_CTRL_ro_pta_en);
	pta_arb_en = GDL_HW_GET_CONN_INFRA_ENTRY(CONN_PTA6_RO_PTA_CTRL_ro_en_pta_arb);

	if (!((pta_en == 1) && (pta_arb_en == 1))) {
		/* should not happen, do nothing but just show log */
		GDL_LOGE("pta_en = %d, pta_arb_en = %d, fail", pta_en, pta_arb_en);
	} else
		GDL_LOGI("pta_en = %d, pta_arb_en = %d, okay", pta_en, pta_arb_en);
}

void gps_dl_hw_deinit_pta(void)
{
	/* Currently no need to do pta deinit */
}


void gps_dl_hw_claim_pta_used_by_gps(void)
{
	/* Currently it's empty */
}

void gps_dl_hw_disclaim_pta_used_by_gps(void)
{
	/* Currently it's empty */
}

void gps_dl_hw_set_pta_blanking_parameter(bool use_direct_path)
{
	if (use_direct_path) {
		/* Use direct path - just cfg src, no other parameter */
		GDL_HW_SET_CONN_INFRA_ENTRY(CONN_PTA6_GPS_BLANK_CFG_r_gps_blank_src, 1);
		return;
	}

	/* Set timeout threashold: ms */
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_PTA6_TMR_CTRL_3_r_gps_l5_blank_tmr_thld, 3);
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_PTA6_TMR_CTRL_3_r_gps_l1_blank_tmr_thld, 3);

	/* Set blanking source: both LTE and NR */
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_PTA6_GPS_BLANK_CFG_r_idc_gps_l1_blank_src, 2);
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_PTA6_GPS_BLANK_CFG_r_idc_gps_l5_blank_src, 2);

	/* Use IDC mode */
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_PTA6_GPS_BLANK_CFG_r_gps_blank_src, 0);
}

/*
 * COS_SEMA_COEX_INDEX = 5(see conninfra/platform/include/consys_hw.h)
 * GPS use M3
 */
#define COS_SEMA_COEX_STA_ENTRY_FOR_GPS \
	CONN_SEMAPHORE_CONN_SEMA05_M3_OWN_STA_CONN_SEMA05_M3_OWN_STA

#define COS_SEMA_COEX_REL_ENTRY_FOR_GPS \
	CONN_SEMAPHORE_CONN_SEMA05_M3_OWN_REL_CONN_SEMA05_M3_OWN_REL

bool gps_dl_hw_take_conn_coex_hw_sema(unsigned int try_timeout_ms)
{
	bool okay;
	bool show_log;
	unsigned int poll_us, poll_max_us;
	unsigned int val;

	show_log = gps_dl_set_show_reg_rw_log(true);
	/* poll until value is expected or timeout */
	poll_us = 0;
	poll_max_us = POLL_US * 1000 * try_timeout_ms;
	okay = false;
	while (!okay) {
		val = GDL_HW_GET_CONN_INFRA_ENTRY(COS_SEMA_COEX_STA_ENTRY_FOR_GPS);
		/* 2bit value:
		 * 0 -> need waiting
		 * 1,3 -> okay; 2 -> already taken
		 */
		if (val != 0) {
			okay = true;
			break;
		}
		if (poll_us >= poll_max_us) {
			okay = false;
			break;
		}
		gps_dl_wait_us(POLL_INTERVAL_US);
		poll_us += POLL_INTERVAL_US;
	}
	gps_dl_set_show_reg_rw_log(show_log);

	if (!okay)
		GDL_LOGW("okay = %d", okay);
	else
		GDL_LOGD("okay = %d", okay);
	return okay;
}

void gps_dl_hw_give_conn_coex_hw_sema(void)
{
	bool show_log;

	show_log = gps_dl_set_show_reg_rw_log(true);
	GDL_HW_SET_CONN_INFRA_ENTRY(COS_SEMA_COEX_REL_ENTRY_FOR_GPS, 1);
	gps_dl_set_show_reg_rw_log(show_log);

	GDL_LOGD("");
}
#endif /* GPS_DL_HAS_PTA */

/*
 * COS_SEMA_RFSPI_INDEX = 11(see conninfra/platform/include/consys_hw.h)
 * GPS use M3
 */
#define COS_SEMA_RFSPI_STA_ENTRY_FOR_GPS \
	CONN_SEMAPHORE_CONN_SEMA11_M3_OWN_STA_CONN_SEMA11_M3_OWN_STA

#define COS_SEMA_RFSPI_REL_ENTRY_FOR_GPS \
	CONN_SEMAPHORE_CONN_SEMA11_M3_OWN_REL_CONN_SEMA11_M3_OWN_REL

bool gps_dl_hw_take_conn_rfspi_hw_sema(unsigned int try_timeout_ms)
{
	bool okay;
	bool show_log;
	unsigned int poll_us, poll_max_us;
	unsigned int val;

	show_log = gps_dl_set_show_reg_rw_log(true);
	/* poll until value is expected or timeout */
	poll_us = 0;
	poll_max_us = POLL_US * 1000 * try_timeout_ms;
	okay = false;
	while (!okay) {
		val = GDL_HW_GET_CONN_INFRA_ENTRY(COS_SEMA_RFSPI_STA_ENTRY_FOR_GPS);
		/* 2bit value:
		 * 0 -> need waiting
		 * 1,3 -> okay; 2 -> already taken
		 */
		if (val != 0) {
			okay = true;
			break;
		}
		if (poll_us >= poll_max_us) {
			okay = false;
			break;
		}
		gps_dl_wait_us(POLL_INTERVAL_US);
		poll_us += POLL_INTERVAL_US;
	}
	gps_dl_set_show_reg_rw_log(show_log);

	GDL_LOGI("okay = %d", okay);

	return okay;
}

void gps_dl_hw_give_conn_rfspi_hw_sema(void)
{
	bool show_log;

	show_log = gps_dl_set_show_reg_rw_log(true);
	GDL_HW_SET_CONN_INFRA_ENTRY(COS_SEMA_RFSPI_REL_ENTRY_FOR_GPS, 1);
	gps_dl_set_show_reg_rw_log(show_log);

	GDL_LOGI("");
}


#define GPS_DL_RFSPI_BUSY_POLL_MAX (10*1000*POLL_US) /* 10ms */

/* note: must be protect by gps_dl_hw_take_conn_rfspi_hw_sema */
static bool gps_dl_hw_gps_fmspi_state_backup(unsigned int *p_rd_ext_en_bk, unsigned int *p_rd_ext_cnt_bk)
{
	bool okay = true;
	bool poll_okay;

	if (p_rd_ext_en_bk == NULL || p_rd_ext_cnt_bk == NULL)
		return false;

	GDL_HW_POLL_CONN_INFRA_ENTRY(CONN_RF_SPI_MST_REG_SPI_STA_FM_BUSY, 0,
		GPS_DL_RFSPI_BUSY_POLL_MAX, &poll_okay);
	if (!poll_okay)
		return false;

	*p_rd_ext_en_bk = GDL_HW_GET_CONN_INFRA_ENTRY(CONN_RF_SPI_MST_REG_FM_CTRL_FM_RD_EXT_EN);
	*p_rd_ext_cnt_bk = GDL_HW_GET_CONN_INFRA_ENTRY(CONN_RF_SPI_MST_REG_FM_CTRL_FM_RD_EXT_CNT);
	return okay;
}

/* note: must be protect by gps_dl_hw_take_conn_rfspi_hw_sema */
static void gps_dl_hw_gps_fmspi_state_restore(unsigned int rd_ext_en_bk, unsigned int rd_ext_cnt_bk)
{
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_RF_SPI_MST_REG_FM_CTRL_FM_RD_EXT_EN, rd_ext_en_bk);
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_RF_SPI_MST_REG_FM_CTRL_FM_RD_EXT_CNT, rd_ext_cnt_bk);
}

/* note: must be protect by gps_dl_hw_take_conn_rfspi_hw_sema */
static bool gps_dl_hw_gps_fmspi_read_rfcr(unsigned int addr, unsigned int *p_val)
{
	unsigned int val;
	bool okay;
	bool poll_okay;
	unsigned int tmp;

	if (p_val == NULL)
		return false;

	GDL_HW_POLL_CONN_INFRA_ENTRY(CONN_RF_SPI_MST_REG_SPI_STA_FM_BUSY, 0,
		GPS_DL_RFSPI_BUSY_POLL_MAX, &poll_okay);
	if (!poll_okay)
		return false;

	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_RF_SPI_MST_REG_FM_CTRL_FM_RD_EXT_EN, 0);
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_RF_SPI_MST_REG_FM_CTRL_FM_RD_EXT_CNT, 0);

	tmp = ((addr & 0xFFF) | (1 << 12UL) | (4 << 13UL) | (0 << 16UL));
	GDL_HW_WR_CONN_INFRA_REG(CONN_RF_SPI_MST_ADDR_SPI_FM_ADDR_ADDR, tmp);
	GDL_HW_WR_CONN_INFRA_REG(CONN_RF_SPI_MST_ADDR_SPI_FM_WDAT_ADDR, 0);

	GDL_HW_POLL_CONN_INFRA_ENTRY(CONN_RF_SPI_MST_REG_SPI_STA_FM_BUSY, 0,
		GPS_DL_RFSPI_BUSY_POLL_MAX, &poll_okay);
	if (!poll_okay) {
		GDL_HW_RD_CONN_INFRA_REG(CONN_RF_SPI_MST_ADDR_SPI_FM_RDAT_ADDR);
		return false;
	}

	val = GDL_HW_RD_CONN_INFRA_REG(CONN_RF_SPI_MST_ADDR_SPI_FM_RDAT_ADDR);
	*p_val = val;
	okay = true;
	return okay;
}

static bool gps_dl_hw_gps_fmspi_write_rfcr(unsigned int addr, unsigned int val)
{
	bool okay;
	bool poll_okay;
	unsigned int tmp;

	GDL_HW_POLL_CONN_INFRA_ENTRY(CONN_RF_SPI_MST_REG_SPI_STA_FM_BUSY, 0,
		GPS_DL_RFSPI_BUSY_POLL_MAX, &poll_okay);
	if (!poll_okay)
		return false;

	tmp = ((addr & 0xFFF) | (0 << 12UL) | (4 << 13UL) | (0 << 16UL));
	GDL_HW_WR_CONN_INFRA_REG(CONN_RF_SPI_MST_ADDR_SPI_FM_ADDR_ADDR, tmp);
	GDL_HW_WR_CONN_INFRA_REG(CONN_RF_SPI_MST_ADDR_SPI_FM_WDAT_ADDR, val);

	GDL_HW_POLL_CONN_INFRA_ENTRY(CONN_RF_SPI_MST_REG_SPI_STA_FM_BUSY, 0,
		GPS_DL_RFSPI_BUSY_POLL_MAX, &poll_okay);
	if (!poll_okay) {
		GDL_HW_RD_CONN_INFRA_REG(CONN_RF_SPI_MST_ADDR_SPI_FM_RDAT_ADDR);
		return false;
	}

	okay = true;
	return okay;
}

void gps_dl_hw_gps_dump_gps_rf_cr(void)
{
	bool show_log, backup_okay;
	unsigned int addr, val;
	unsigned int rd_ext_en_bk, rd_ext_cnt_bk;

	gps_dl_hw_take_conn_rfspi_hw_sema(100);
	show_log = gps_dl_set_show_reg_rw_log(true);
	backup_okay = gps_dl_hw_gps_fmspi_state_backup(&rd_ext_en_bk, &rd_ext_cnt_bk);

	/* read: 0x500 ~ 0x51b */
	for (addr = 0x500; addr <= 0x51B; addr++) {
		if (gps_dl_hw_gps_fmspi_read_rfcr(addr, &val))
			GDL_LOGW("rd: addr = 0x%x, val = 0x%x", addr, val);
		else
			GDL_LOGW("rd: addr = 0x%x, fail", addr);
	}

	/* write: 0x51a = 1 */
	addr = 0x51A;
	val = 1;
	if (gps_dl_hw_gps_fmspi_write_rfcr(addr, val))
		GDL_LOGW("wr: addr = 0x%x, val = 0x%x, okay", addr, val);
	else
		GDL_LOGW("wr: addr = 0x%x, val = 0x%x, fail", addr, val);

	/* read: 0x51a & 0x51b */
	for (addr = 0x51A; addr <= 0x51B; addr++) {
		if (gps_dl_hw_gps_fmspi_read_rfcr(addr, &val))
			GDL_LOGW("rd: addr = 0x%x, val = 0x%x", addr, val);
		else
			GDL_LOGW("rd: addr = 0x%x, fail", addr);
	}

	if (backup_okay)
		gps_dl_hw_gps_fmspi_state_restore(rd_ext_en_bk, rd_ext_cnt_bk);
	else
		GDL_LOGW("not do gps_dl_hw_gps_fmspi_state_restore due to backup failed!");
	gps_dl_set_show_reg_rw_log(show_log);
	gps_dl_hw_give_conn_rfspi_hw_sema();
}

