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

#include "gps_dl_hal.h"
#if GPS_DL_MOCK_HAL
#include "gps_mock_hal.h"
#endif
#if GPS_DL_HAS_CONNINFRA_DRV
#if GPS_DL_ON_LINUX
#include "conninfra.h"
#elif GPS_DL_ON_CTP
#include "conninfra_ext.h"
#endif
#endif

#include "gps_dl_hw_api.h"
#include "gps_dl_hw_dep_api.h"
#include "gps_dl_hw_priv_util.h"
#include "gps_dl_hal_util.h"
#include "gps_dsp_fsm.h"
#include "gps_dl_subsys_reset.h"

#include "conn_infra/conn_infra_rgu.h"
#include "conn_infra/conn_infra_cfg.h"
#include "conn_infra/conn_host_csr_top.h"

#include "gps/gps_rgu_on.h"
#include "gps/gps_cfg_on.h"
#include "gps/gps_aon_top.h"
#include "gps/bgf_gps_cfg.h"

static int gps_dl_hw_gps_sleep_prot_ctrl(int op)
{
	bool poll_okay;

	if (1 == op) {
		/* disable when on */
		GDL_HW_SET_CONN2GPS_SLP_PROT_RX_VAL(0);
		GDL_HW_POLL_CONN2GPS_SLP_PROT_RX_UNTIL_VAL(0, POLL_DEFAULT, &poll_okay);
		if (!poll_okay) {
			GDL_LOGE("_fail_disable_gps_slp_prot - conn2gps rx");
			goto _fail_disable_gps_slp_prot;
		}

		GDL_HW_SET_CONN2GPS_SLP_PROT_TX_VAL(0);
		GDL_HW_POLL_CONN2GPS_SLP_PROT_TX_UNTIL_VAL(0, POLL_DEFAULT, &poll_okay);
		if (!poll_okay) {
			GDL_LOGE("_fail_disable_gps_slp_prot - conn2gps tx");
			goto _fail_disable_gps_slp_prot;
		}

		GDL_HW_SET_GPS2CONN_SLP_PROT_RX_VAL(0);
		GDL_HW_POLL_GPS2CONN_SLP_PROT_RX_UNTIL_VAL(0, POLL_DEFAULT, &poll_okay);
		if (!poll_okay) {
			GDL_LOGE("_fail_disable_gps_slp_prot - gps2conn rx");
			goto _fail_disable_gps_slp_prot;
		}

		GDL_HW_SET_GPS2CONN_SLP_PROT_TX_VAL(0);
		GDL_HW_POLL_GPS2CONN_SLP_PROT_TX_UNTIL_VAL(0, POLL_DEFAULT, &poll_okay);
		if (!poll_okay) {
			GDL_LOGE("_fail_disable_gps_slp_prot - gps2conn tx");
			goto _fail_disable_gps_slp_prot;
		}
		return 0;

_fail_disable_gps_slp_prot:
#if 0
		GDL_HW_WR_CONN_INFRA_REG(CONN_INFRA_CFG_GALS_GPS2CONN_SLP_CTRL_ADDR,
			CONN_INFRA_CFG_GALS_CONN2GPS_SLP_CTRL_R_CONN2GPS_SLP_PROT_RX_EN_MASK |
			CONN_INFRA_CFG_GALS_CONN2GPS_SLP_CTRL_R_CONN2GPS_SLP_PROT_TX_EN_MASK |
			CONN_INFRA_CFG_GALS_GPS2CONN_SLP_CTRL_R_GPS2CONN_SLP_PROT_RX_EN_MASK |
			CONN_INFRA_CFG_GALS_GPS2CONN_SLP_CTRL_R_GPS2CONN_SLP_PROT_TX_EN_MASK);
#endif
		return -1;
	} else if (0 == op) {
		/* enable when off */
		GDL_HW_SET_CONN2GPS_SLP_PROT_TX_VAL(1);
		GDL_HW_POLL_CONN2GPS_SLP_PROT_TX_UNTIL_VAL(1, POLL_DEFAULT, &poll_okay);
		if (!poll_okay) {
			/* From DE: need to trigger connsys reset */
			GDL_LOGE("_fail_enable_gps_slp_prot - conn2gps tx");
			goto _fail_enable_gps_slp_prot;
		}

		GDL_HW_SET_CONN2GPS_SLP_PROT_RX_VAL(1);
		GDL_HW_POLL_CONN2GPS_SLP_PROT_RX_UNTIL_VAL(1, POLL_DEFAULT, &poll_okay);
		if (!poll_okay) {
			/* not handle it, just show warning */
			GDL_LOGE("_fail_enable_gps_slp_prot - conn2gps rx");
		}

		GDL_HW_SET_GPS2CONN_SLP_PROT_TX_VAL(1);
		GDL_HW_POLL_GPS2CONN_SLP_PROT_TX_UNTIL_VAL(1, POLL_DEFAULT, &poll_okay);
		if (!poll_okay) {
			/* not handle it, just show warning */
			GDL_LOGE("_fail_enable_gps_slp_prot - gps2conn tx");
		}

		GDL_HW_SET_GPS2CONN_SLP_PROT_RX_VAL(1);
		GDL_HW_POLL_GPS2CONN_SLP_PROT_RX_UNTIL_VAL(1, POLL_DEFAULT, &poll_okay);
		if (!poll_okay) {
			/* From DE: need to trigger connsys reset */
			GDL_LOGE("_fail_enable_gps_slp_prot - gps2conn rx");
			goto _fail_enable_gps_slp_prot;
		}

		return 0;

_fail_enable_gps_slp_prot:
		/* trigger reset on outer function */
#if 0
		gps_dl_trigger_connsys_reset();
#endif
		return -1;
	}

	return 0;
}

bool gps_dl_hw_gps_force_wakeup_conninfra_top_off(bool enable)
{
	bool poll_okay;

	if (enable) {
		GDL_HW_SET_CONN_INFRA_ENTRY(CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_GPS_CONN_INFRA_WAKEPU_GPS, 1);
		GDL_HW_MAY_WAIT_CONN_INFRA_SLP_PROT_DISABLE_ACK(&poll_okay);
		if (!poll_okay) {
			GDL_LOGE("_fail_conn_slp_prot_not_okay");
			return false; /* not okay */
		}
	} else
		GDL_HW_SET_CONN_INFRA_ENTRY(CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_GPS_CONN_INFRA_WAKEPU_GPS, 0);

	return true;
}

void gps_dl_hw_gps_sw_request_emi_usage(bool request)
{
	bool show_log;
	bool reg_rw_log = gps_dl_log_reg_rw_is_on(GPS_DL_REG_RW_EMI_SW_REQ_CTRL);

	if (reg_rw_log) {
		show_log = gps_dl_set_show_reg_rw_log(true);
		GDL_HW_RD_CONN_INFRA_REG(CONN_INFRA_CFG_EMI_CTL_TOP_ADDR);
		GDL_HW_RD_CONN_INFRA_REG(CONN_INFRA_CFG_EMI_CTL_WF_ADDR);
		GDL_HW_RD_CONN_INFRA_REG(CONN_INFRA_CFG_EMI_CTL_BT_ADDR);
		GDL_HW_RD_CONN_INFRA_REG(CONN_INFRA_CFG_EMI_CTL_GPS_ADDR);
	}
#if (GPS_DL_USE_TIA)
	/* If use TIA, CONN_INFRA_CFG_EMI_CTL_GPS used by DSP, driver use TOP's. */
	if (request)
		GDL_HW_SET_CONN_INFRA_ENTRY(CONN_INFRA_CFG_EMI_CTL_TOP_EMI_REQ_TOP, 1);
	else
		GDL_HW_SET_CONN_INFRA_ENTRY(CONN_INFRA_CFG_EMI_CTL_TOP_EMI_REQ_TOP, 0);
#else
	if (request)
		GDL_HW_SET_CONN_INFRA_ENTRY(CONN_INFRA_CFG_EMI_CTL_GPS_EMI_REQ_GPS, 1);
	else
		GDL_HW_SET_CONN_INFRA_ENTRY(CONN_INFRA_CFG_EMI_CTL_GPS_EMI_REQ_GPS, 0);
#endif
	if (reg_rw_log)
		gps_dl_set_show_reg_rw_log(show_log);
}

int gps_dl_hw_gps_common_on(void)
{
	bool poll_okay;
	unsigned int poll_ver;
	int i;

	/* Enable Conninfra BGF */
	GDL_HW_SET_CONN_INFRA_BGF_EN(1);

	/* Poll conninfra hw version */
	GDL_HW_CHECK_CONN_INFRA_VER(&poll_okay, &poll_ver);
	if (!poll_okay) {
		GDL_LOGE("_fail_conn_hw_ver_not_okay, poll_ver = 0x%08x", poll_ver);
		goto _fail_conn_hw_ver_not_okay;
	}

	/* GDL_HW_CHECK_CONN_INFRA_VER may check a list and return ok if poll_ver is in the list,
	 * record the poll_ver here and we can know which one it is,
	 * and it may help for debug purpose.
	 */
	gps_dl_hal_set_conn_infra_ver(poll_ver);
	GDL_LOGW("%s: poll_ver = 0x%08x is ok", GDL_HW_SUPPORT_LIST, poll_ver);

	/* GPS SW EMI request
	 * gps_dl_hw_gps_sw_request_emi_usage(true);
	 */
	gps_dl_hal_emi_usage_init();

	/* Enable GPS function */
	GDL_HW_SET_GPS_FUNC_EN(1);

	/* bit24: BGFSYS_ON_TOP primary power ack */
	GDL_HW_POLL_CONN_INFRA_ENTRY(CONN_INFRA_RGU_BGFSYS_ON_TOP_PWR_ACK_ST_BGFSYS_ON_TOP_PWR_ACK, 1,
		POLL_DEFAULT, &poll_okay);
	if (!poll_okay) {
		GDL_LOGE("_fail_bgf_top_1st_pwr_ack_not_okay");
		goto _fail_bgf_top_1st_pwr_ack_not_okay;
	}

	/* bit25: BGFSYS_ON_TOP secondary power ack */
	GDL_HW_POLL_CONN_INFRA_ENTRY(CONN_INFRA_RGU_BGFSYS_ON_TOP_PWR_ACK_ST_AN_BGFSYS_ON_TOP_PWR_ACK_S, 1,
		POLL_DEFAULT, &poll_okay);
	if (!poll_okay) {
		GDL_LOGE("_fail_bgf_top_2nd_pwr_ack_not_okay");
		goto _fail_bgf_top_2nd_pwr_ack_not_okay;
	}

	GDL_WAIT_US(200);

	/* sleep prot */
	if (gps_dl_hw_gps_sleep_prot_ctrl(1) != 0) {
		GDL_LOGE("_fail_disable_gps_slp_prot_not_okay");
		goto _fail_disable_gps_slp_prot_not_okay;
	}

	/* polling status and version */
	/* Todo: set GPS host csr flag selection */
	/* 0x18060240[3:0] == 4h'2 gps_top_off is GPS_ACTIVE state */
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_HOST_CSR_TOP_HOST2GPS_DEGUG_SEL_HOST2GPS_DEGUG_SEL, 0x80);
	for (i = 0; i < 3; i++) {
		GDL_HW_POLL_CONN_INFRA_ENTRY(CONN_HOST_CSR_TOP_GPS_CFG2HOST_DEBUG_GPS_CFG2HOST_DEBUG, 2,
			POLL_DEFAULT, &poll_okay);
		if (poll_okay)
			break;
		/*
		 * TODO:
		 * if (!gps_dl_reset_level_is_none()) break;
		 */
		if (i > 0)
			GDL_LOGW("_poll_gps_top_off_active, cnt = %d", i + 1);
	}
	if (!poll_okay) {
		GDL_LOGE("_fail_gps_top_off_active_not_okay");
		goto _fail_gps_top_off_active_not_okay;
	}

	/* 0x18c21010[31:0] bgf ip version */
	GDL_HW_POLL_GPS_ENTRY(BGF_GPS_CFG_BGF_IP_VERSION_BGFSYS_VERSION,
		GDL_HW_BGF_VER, POLL_DEFAULT, &poll_okay);
	if (!poll_okay) {
		GDL_LOGE("_fail_bgf_ip_ver_not_okay");
		goto _fail_bgf_ip_ver_not_okay;
	}

	/* 0x18c21014[7:0] bgf ip cfg */
	GDL_HW_POLL_GPS_ENTRY(BGF_GPS_CFG_BGF_IP_CONFIG_BGFSYS_CONFIG, 0,
		POLL_DEFAULT, &poll_okay);
	if (!poll_okay) {
		GDL_LOGE("_fail_bgf_ip_cfg_not_okay");
		goto _fail_bgf_ip_cfg_not_okay;
	}

#if (GPS_DL_HAS_CONNINFRA_DRV)
	/* conninfra driver add an API to do bellow step */
	conninfra_config_setup();
#else
	/* Enable conninfra bus hung detection */
	GDL_HW_WR_CONN_INFRA_REG(0x1800F000, 0xF000001C);
#endif

	/* host csr gps bus debug mode enable 0x18c60000 = 0x10 */
	GDL_HW_WR_GPS_REG(0x80060000, 0x10);

	/* Power on A-die top clock */
#if (GPS_DL_HAS_CONNINFRA_DRV)
	conninfra_adie_top_ck_en_on(CONNSYS_ADIE_CTL_HOST_GPS);
#endif

	/* Enable PLL driver */
	GDL_HW_SET_GPS_ENTRY(GPS_CFG_ON_GPS_CLKGEN1_CTL_CR_GPS_DIGCK_DIV_EN, 1);

	return 0;

_fail_bgf_ip_cfg_not_okay:
_fail_bgf_ip_ver_not_okay:
_fail_gps_top_off_active_not_okay:
_fail_disable_gps_slp_prot_not_okay:
_fail_bgf_top_2nd_pwr_ack_not_okay:
_fail_bgf_top_1st_pwr_ack_not_okay:
	GDL_HW_SET_GPS_FUNC_EN(0);
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_INFRA_CFG_EMI_CTL_GPS_EMI_REQ_GPS, 0);

_fail_conn_hw_ver_not_okay:
	return -1;
}

int gps_dl_hw_gps_common_off(void)
{
	/* Power off A-die top clock */
#if (GPS_DL_HAS_CONNINFRA_DRV)
	conninfra_adie_top_ck_en_off(CONNSYS_ADIE_CTL_HOST_GPS);
#endif

	if (gps_dl_hw_gps_sleep_prot_ctrl(0) != 0) {
		GDL_LOGE("enable sleep prot fail, trigger connsys reset");
		gps_dl_trigger_connsys_reset();
		return -1;
	}

	/* GPS SW EMI request
	 * gps_dl_hw_gps_sw_request_emi_usage(false);
	 */
	gps_dl_hal_emi_usage_deinit();

	if (gps_dl_log_reg_rw_is_on(GPS_DL_REG_RW_HOST_CSR_GPS_OFF))
		gps_dl_hw_dump_host_csr_conninfra_info(true);

	/* Disable GPS function */
	GDL_HW_SET_GPS_FUNC_EN(0);

	/* Disable Conninfra BGF */
	GDL_HW_SET_CONN_INFRA_BGF_EN(0);

	return 0;
}

/* L1 and L5 share same pwr stat and current we can support the bellow case:
 * 1. Both L1 and L5 on / off
 * 2. Both L1 and L5 enter deep stop mode and wakeup
 * 3. L5 stays off, L1 do on / off
 * 4. L5 stays off, L1 enter deep stop mode and wakeup
 */
unsigned int g_gps_pwr_stat;
int gps_dl_hw_gps_pwr_stat_ctrl(enum dsp_ctrl_enum ctrl)
{
	bool clk_ext = gps_dl_hal_get_need_clk_ext_flag(GPS_DATA_LINK_ID0);

	switch (ctrl) {
	case GPS_L1_DSP_ON:
	case GPS_L5_DSP_ON:
	case GPS_L1_DSP_OFF:
	case GPS_L5_DSP_OFF:
		GDL_HW_SET_GPS_ENTRY(GPS_AON_TOP_DSLEEP_CTL_GPS_PWR_STAT, 0);
		GDL_HW_SET_GPS_ENTRY(GPS_AON_TOP_DSLEEP_CTL_FORCE_OSC_EN_ON, 0);
		g_gps_pwr_stat = 0;
		break;

	case GPS_L1_DSP_CLEAR_PWR_STAT:
	case GPS_L5_DSP_CLEAR_PWR_STAT:
		gps_dl_hw_print_ms_counter_status();
		GDL_HW_SET_GPS_ENTRY(GPS_AON_TOP_DSLEEP_CTL_GPS_PWR_STAT, 0);
		GDL_HW_SET_GPS_ENTRY(GPS_AON_TOP_DSLEEP_CTL_FORCE_OSC_EN_ON, 0);
		g_gps_pwr_stat = 0;
		break;

	case GPS_L1_DSP_ENTER_DSTOP:
	case GPS_L5_DSP_ENTER_DSTOP:
		GDL_HW_SET_GPS_ENTRY(GPS_AON_TOP_DSLEEP_CTL_GPS_PWR_STAT, 1);
		GDL_HW_SET_GPS_ENTRY(GPS_AON_TOP_DSLEEP_CTL_FORCE_OSC_EN_ON, clk_ext);
		gps_dl_hw_print_ms_counter_status();
		g_gps_pwr_stat = 1;
		break;

	case GPS_L1_DSP_EXIT_DSTOP:
	case GPS_L5_DSP_EXIT_DSTOP:
		/* do nothing */
		gps_dl_hw_print_ms_counter_status();
		break;

	case GPS_L1_DSP_ENTER_DSLEEP:
	case GPS_L5_DSP_ENTER_DSLEEP:
		GDL_HW_SET_GPS_ENTRY(GPS_AON_TOP_DSLEEP_CTL_GPS_PWR_STAT, 3);
		GDL_HW_SET_GPS_ENTRY(GPS_AON_TOP_DSLEEP_CTL_FORCE_OSC_EN_ON, clk_ext);
		g_gps_pwr_stat = 3;
		break;

	case GPS_L1_DSP_EXIT_DSLEEP:
	case GPS_L5_DSP_EXIT_DSLEEP:
		/* do nothong */
		break;

	default:
		break;
	}

	return 0;
}

int gps_dl_hw_gps_dsp_ctrl(enum dsp_ctrl_enum ctrl)
{
	bool poll_okay;
	bool dsp_off_done;

	switch (ctrl) {
	case GPS_L1_DSP_ON:
	case GPS_L1_DSP_EXIT_DSTOP:
	case GPS_L1_DSP_EXIT_DSLEEP:
		gps_dl_hw_gps_pwr_stat_ctrl(ctrl);
		GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_CR_RGU_GPS_L1_ON, 1);
		GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_CR_RGU_GPS_L1_SOFT_RST_B, 1);
		GDL_HW_POLL_GPS_ENTRY(GPS_CFG_ON_GPS_L1_SLP_PWR_CTL_GPS_L1_SLP_PWR_CTL_CS, 3,
			POLL_DEFAULT, &poll_okay);
		if (!poll_okay) {
			GDL_LOGE("ctrl = %d fail", ctrl);
			return -1;
		}

		GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_MEM_DLY_CTL_RGU_GPSSYS_L1_MEM_ADJ_DLY_EN, 1);
		GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DLY_CHAIN_CTL_RGU_GPS_L1_MEM_PDN_DELAY_DUMMY_NUM, 5);
		gps_dl_wait_us(100); /* 3 x 32k clk ~= 100us */
		gps_dl_hw_usrt_ctrl(GPS_DATA_LINK_ID0,
			true, gps_dl_is_dma_enabled(), gps_dl_is_1byte_mode());
		break;

	case GPS_L1_DSP_CLEAR_PWR_STAT:
		gps_dl_hw_gps_pwr_stat_ctrl(ctrl);
		return 0;

	case GPS_L1_DSP_OFF:
	case GPS_L1_DSP_ENTER_DSTOP:
	case GPS_L1_DSP_ENTER_DSLEEP:
		gps_dl_hw_usrt_ctrl(GPS_DATA_LINK_ID0,
			false, gps_dl_is_dma_enabled(), gps_dl_is_1byte_mode());

		/* poll */
		dsp_off_done = gps_dl_hw_gps_dsp_is_off_done(GPS_DATA_LINK_ID0);

		gps_dl_hw_gps_pwr_stat_ctrl(ctrl);
		if (ctrl == GPS_L1_DSP_ENTER_DSLEEP) {
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DSPPRAM_PDN_EN_RGU_GPS_L1_PRAM_HWCTL_PDN, 0);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DSPPRAM_SLP_EN_RGU_GPS_L1_PRAM_HWCTL_SLP, 0x1FF);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DSPXRAM_PDN_EN_RGU_GPS_L1_XRAM_HWCTL_PDN, 0);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DSPXRAM_SLP_EN_RGU_GPS_L1_XRAM_HWCTL_SLP, 0xF);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DSPYRAM_PDN_EN_RGU_GPS_L1_YRAM_HWCTL_PDN, 0);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DSPYRAM_SLP_EN_RGU_GPS_L1_YRAM_HWCTL_SLP, 0x1FF);
		} else if (ctrl == GPS_L1_DSP_ENTER_DSTOP) {
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DSPPRAM_PDN_EN_RGU_GPS_L1_PRAM_HWCTL_PDN, 0);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DSPPRAM_SLP_EN_RGU_GPS_L1_PRAM_HWCTL_SLP, 0x1FF);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DSPXRAM_PDN_EN_RGU_GPS_L1_XRAM_HWCTL_PDN, 0);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DSPXRAM_SLP_EN_RGU_GPS_L1_XRAM_HWCTL_SLP, 0xF);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DSPYRAM_PDN_EN_RGU_GPS_L1_YRAM_HWCTL_PDN, 0x1FF);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DSPYRAM_SLP_EN_RGU_GPS_L1_YRAM_HWCTL_SLP, 0);
		} else {
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DSPPRAM_PDN_EN_RGU_GPS_L1_PRAM_HWCTL_PDN, 0x1FF);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DSPPRAM_SLP_EN_RGU_GPS_L1_PRAM_HWCTL_SLP, 0);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DSPXRAM_PDN_EN_RGU_GPS_L1_XRAM_HWCTL_PDN, 0xF);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DSPXRAM_SLP_EN_RGU_GPS_L1_XRAM_HWCTL_SLP, 0);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DSPYRAM_PDN_EN_RGU_GPS_L1_YRAM_HWCTL_PDN, 0x1FF);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_DSPYRAM_SLP_EN_RGU_GPS_L1_YRAM_HWCTL_SLP, 0);
		}

		GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_CR_RGU_GPS_L1_SOFT_RST_B, 0);
		GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L1_CR_RGU_GPS_L1_ON, 0);

		if (dsp_off_done)
			return 0;
		else
			return -1;

	case GPS_L5_DSP_ON:
	case GPS_L5_DSP_EXIT_DSTOP:
	case GPS_L5_DSP_EXIT_DSLEEP:
		gps_dl_hw_gps_pwr_stat_ctrl(ctrl);

		GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_CR_RGU_GPS_L5_ON, 1);
		GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_CR_RGU_GPS_L5_SOFT_RST_B, 1);
		GDL_HW_POLL_GPS_ENTRY(GPS_CFG_ON_GPS_L5_SLP_PWR_CTL_GPS_L5_SLP_PWR_CTL_CS, 3,
			POLL_DEFAULT, &poll_okay);
		if (!poll_okay) {
			GDL_LOGE("ctrl = %d fail", ctrl);
			return -1;
		}

		GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_MEM_DLY_CTL_RGU_GPSSYS_L5_MEM_ADJ_DLY_EN, 1);
		GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DLY_CHAIN_CTL_RGU_GPS_L5_MEM_PDN_DELAY_DUMMY_NUM, 9);
		gps_dl_wait_us(100); /* 3 x 32k clk ~= 1ms */
		gps_dl_hw_usrt_ctrl(GPS_DATA_LINK_ID1,
			true, gps_dl_is_dma_enabled(), gps_dl_is_1byte_mode());
		break;

	case GPS_L5_DSP_CLEAR_PWR_STAT:
		gps_dl_hw_gps_pwr_stat_ctrl(ctrl);
		return 0;

	case GPS_L5_DSP_OFF:
	case GPS_L5_DSP_ENTER_DSTOP:
	case GPS_L5_DSP_ENTER_DSLEEP:
		gps_dl_hw_usrt_ctrl(GPS_DATA_LINK_ID1,
			false, gps_dl_is_dma_enabled(), gps_dl_is_1byte_mode());

		/* poll */
		dsp_off_done = gps_dl_hw_gps_dsp_is_off_done(GPS_DATA_LINK_ID1);

		gps_dl_hw_gps_pwr_stat_ctrl(ctrl);
		if (ctrl == GPS_L5_DSP_ENTER_DSLEEP) {
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DSPPRAM_PDN_EN_RGU_GPS_L5_PRAM_HWCTL_PDN, 0);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DSPPRAM_SLP_EN_RGU_GPS_L5_PRAM_HWCTL_SLP, 0x3FF);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DSPXRAM_PDN_EN_RGU_GPS_L5_XRAM_HWCTL_PDN, 0);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DSPXRAM_SLP_EN_RGU_GPS_L5_XRAM_HWCTL_SLP, 0xF);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DSPYRAM_PDN_EN_RGU_GPS_L5_YRAM_HWCTL_PDN, 0);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DSPYRAM_SLP_EN_RGU_GPS_L5_YRAM_HWCTL_SLP, 0x3FF);
		} else if (ctrl == GPS_L5_DSP_ENTER_DSTOP) {
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DSPPRAM_PDN_EN_RGU_GPS_L5_PRAM_HWCTL_PDN, 0);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DSPPRAM_SLP_EN_RGU_GPS_L5_PRAM_HWCTL_SLP, 0x3FF);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DSPXRAM_PDN_EN_RGU_GPS_L5_XRAM_HWCTL_PDN, 0);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DSPXRAM_SLP_EN_RGU_GPS_L5_XRAM_HWCTL_SLP, 0xF);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DSPYRAM_PDN_EN_RGU_GPS_L5_YRAM_HWCTL_PDN, 0x3FF);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DSPYRAM_SLP_EN_RGU_GPS_L5_YRAM_HWCTL_SLP, 0);
		} else {
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DSPPRAM_PDN_EN_RGU_GPS_L5_PRAM_HWCTL_PDN, 0x3FF);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DSPPRAM_SLP_EN_RGU_GPS_L5_PRAM_HWCTL_SLP, 0);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DSPXRAM_PDN_EN_RGU_GPS_L5_XRAM_HWCTL_PDN, 0xF);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DSPXRAM_SLP_EN_RGU_GPS_L5_XRAM_HWCTL_SLP, 0);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DSPYRAM_PDN_EN_RGU_GPS_L5_YRAM_HWCTL_PDN, 0x3FF);
			GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_DSPYRAM_SLP_EN_RGU_GPS_L5_YRAM_HWCTL_SLP, 0);
		}

		GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_CR_RGU_GPS_L5_SOFT_RST_B, 0);
		GDL_HW_SET_GPS_ENTRY(GPS_RGU_ON_GPS_L5_CR_RGU_GPS_L5_ON, 0);

		if (dsp_off_done)
			return 0;
		else
			return -1;

	default:
		return -1;
	}

	return 0;
}

bool gps_dl_hw_gps_dsp_is_off_done(enum gps_dl_link_id_enum link_id)
{
	int i;
	bool done;
	bool show_log;

	/* TODO: move it to proper place */
	if (GPS_DSP_ST_HW_STOP_MODE == gps_dsp_state_get(link_id)) {
		/* expect it change to RESET_DONE after this call */
		if (!gps_dl_hal_mcub_flag_handler(link_id)) {
			GDL_LOGXW(link_id, "pre-check fail");
			return false;
		}
	}

	if (GPS_DSP_ST_RESET_DONE == gps_dsp_state_get(link_id)) {
		GDL_LOGXD(link_id, "1st return, done = 1");
		return true;
	}

	i = 0;

	show_log = gps_dl_set_show_reg_rw_log(true);
	do {
		/* MCUB IRQ already mask at this time */
		if (!gps_dl_hal_mcub_flag_handler(link_id)) {
			done = false;
			break;
		}

		done = true;
		while (GPS_DSP_ST_RESET_DONE != gps_dsp_state_get(link_id)) {
			/* poll 10ms */
			if (i > 10) {
				done = false;
				break;
			}
			gps_dl_wait_us(1000);

			/* read dummy cr confirm dsp state for debug */
			if (GPS_DATA_LINK_ID0 == link_id)
				GDL_HW_RD_GPS_REG(0x80073160);
			else if (GPS_DATA_LINK_ID1 == link_id)
				GDL_HW_RD_GPS_REG(0x80073134);

			if (!gps_dl_hal_mcub_flag_handler(link_id)) {
				done = false;
				break;
			}
			i++;
		}
	} while (0);
	gps_dl_set_show_reg_rw_log(show_log);
	GDL_LOGXW(link_id, "2nd return, done = %d, i = %d", done, i);
	return done;
}

void gps_dl_hw_gps_adie_force_off(void)
{
#if GPS_DL_HAS_CONNINFRA_DRV
	unsigned int spi_data;
	int rd_status;
	int wr_status;

	/* TOP: 0xFC[1:0] = 2'b11 */
	spi_data = 0;
	rd_status = conninfra_spi_read(SYS_SPI_TOP, 0xFC, &spi_data);
	ASSERT_ZERO(rd_status, GDL_VOIDF());
	GDL_LOGW("spi_data = 0x%x", spi_data);
	wr_status = conninfra_spi_write(SYS_SPI_TOP, 0xFC, spi_data | 3UL);
	ASSERT_ZERO(wr_status, GDL_VOIDF());

	/* TOP: 0xA0C[31:0] = 0xFFFFFFFF; 0xAFC[31:0] = 0xFFFFFFFF */
	wr_status = conninfra_spi_write(SYS_SPI_TOP, 0xA0C, 0xFFFFFFFF);
	ASSERT_ZERO(wr_status, GDL_VOIDF());
	wr_status = conninfra_spi_write(SYS_SPI_TOP, 0xAFC, 0xFFFFFFFF);
	ASSERT_ZERO(wr_status, GDL_VOIDF());

	/* TOP: 0xF8[0] = 1'b0 */
	spi_data = 0;
	rd_status = conninfra_spi_read(SYS_SPI_TOP, 0xF8, &spi_data);
	ASSERT_ZERO(rd_status, GDL_VOIDF());
	GDL_LOGW("spi_data = 0x%x", spi_data);
	wr_status = conninfra_spi_write(SYS_SPI_TOP, 0xF8, spi_data & (~1UL));
	ASSERT_ZERO(wr_status, GDL_VOIDF());

	/* GPS: 0x0[15] = 1'b1 */
	spi_data = 0;
	rd_status = conninfra_spi_read(SYS_SPI_GPS, 0x0, &spi_data);
	ASSERT_ZERO(rd_status, GDL_VOIDF());
	GDL_LOGW("spi_data = 0x%x", spi_data);
	wr_status = conninfra_spi_write(SYS_SPI_GPS, 0x0, spi_data & (1UL << 15));
	ASSERT_ZERO(wr_status, GDL_VOIDF());

	/* TOP: 0xF8[0] = 1'b1 */
	spi_data = 0;
	rd_status = conninfra_spi_read(SYS_SPI_TOP, 0xF8, &spi_data);
	ASSERT_ZERO(rd_status, GDL_VOIDF());
	GDL_LOGW("spi_data = 0x%x", spi_data);
	wr_status = conninfra_spi_write(SYS_SPI_TOP, 0xF8, spi_data | 1UL);
	ASSERT_ZERO(wr_status, GDL_VOIDF());

	/* GPS: 0x0[15] = 1'b1 */
	spi_data = 0;
	rd_status = conninfra_spi_read(SYS_SPI_GPS, 0x0, &spi_data);
	ASSERT_ZERO(rd_status, GDL_VOIDF());
	GDL_LOGW("spi_data = 0x%x", spi_data);
	wr_status = conninfra_spi_write(SYS_SPI_GPS, 0x0, spi_data & (1UL << 15));
	ASSERT_ZERO(wr_status, GDL_VOIDF());

	/* TOP: 0xF8[0] = 1'b0 */
	spi_data = 0;
	rd_status = conninfra_spi_read(SYS_SPI_TOP, 0xF8, &spi_data);
	ASSERT_ZERO(rd_status, GDL_VOIDF());
	GDL_LOGW("spi_data = 0x%x", spi_data);
	wr_status = conninfra_spi_write(SYS_SPI_TOP, 0xF8, spi_data & (~1UL));
	ASSERT_ZERO(wr_status, GDL_VOIDF());

	/* TOP: 0xA0C[31:0] = 0; 0xAFC[31:0] = 0 */
	wr_status = conninfra_spi_write(SYS_SPI_TOP, 0xA0C, 0);
	ASSERT_ZERO(wr_status, GDL_VOIDF());
	wr_status = conninfra_spi_write(SYS_SPI_TOP, 0xAFC, 0);
	ASSERT_ZERO(wr_status, GDL_VOIDF());

	/* TOP: 0xFC[1:0] = 2'b00 */
	rd_status = conninfra_spi_read(SYS_SPI_TOP, 0xFC, &spi_data);
	ASSERT_ZERO(rd_status, GDL_VOIDF());
	GDL_LOGW("spi_data = 0x%x", spi_data);
	wr_status = conninfra_spi_write(SYS_SPI_TOP, 0xFC, spi_data & (~3UL));
	ASSERT_ZERO(wr_status, GDL_VOIDF());
#else
	GDL_LOGE("no conninfra driver");
#endif
}

void gps_dl_hw_gps_dump_top_rf_cr(void)
{
#if GPS_DL_HAS_CONNINFRA_DRV
	unsigned int spi_data;
	int rd_status;
	int i;
	const int rd_addr_list[] = {0x03C, 0xA18, 0xA1C, 0x0C8, 0xA00, 0x0B4, 0x34C};
	int rd_addr;

	for (i = 0; i < ARRAY_SIZE(rd_addr_list); i++) {
		rd_addr = rd_addr_list[i];
		spi_data = 0;
		rd_status = conninfra_spi_read(SYS_SPI_TOP, rd_addr, &spi_data);
		GDL_LOGW("rd: addr = 0x%x, val = 0x%x, rd_status = %d",
			rd_addr, spi_data, rd_status);
	}
#else
	GDL_LOGE("no conninfra driver");
#endif
}

