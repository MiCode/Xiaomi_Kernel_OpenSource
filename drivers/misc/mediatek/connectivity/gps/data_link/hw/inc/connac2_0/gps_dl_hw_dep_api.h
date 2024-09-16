/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _GPS_DL_HW_DEP_API_H
#define _GPS_DL_HW_DEP_API_H

#include "gps_dl_hw_ver.h"
#include "conn_infra/conn_infra_cfg.h"
#include "conn_infra/conn_host_csr_top.h"

#define GDL_HW_SUPPORT_LIST "SUPPORT:MT6885,MT6893"

#define GDL_HW_CHECK_CONN_INFRA_VER(p_poll_okay, p_poll_ver)             \
	GDL_HW_POLL_ENTRY_VERBOSE(GPS_DL_CONN_INFRA_BUS,                 \
		CONN_INFRA_CFG_CONN_HW_VER_RO_CONN_HW_VERSION,           \
		p_poll_okay, p_poll_ver, POLL_DEFAULT, (                 \
			(*p_poll_ver == GDL_HW_CONN_INFRA_VER_MT6885) || \
			(*p_poll_ver == GDL_HW_CONN_INFRA_VER_MT6893))   \
	)


#define GDL_HW_SET_CONN2GPS_SLP_PROT_RX_VAL(val) \
	GDL_HW_SET_CONN_INFRA_ENTRY( \
		CONN_INFRA_CFG_GALS_CONN2GPS_SLP_CTRL_R_CONN2GPS_SLP_PROT_RX_EN, val)

#define GDL_HW_POLL_CONN2GPS_SLP_PROT_RX_UNTIL_VAL(val, timeout, p_is_okay) \
	GDL_HW_POLL_CONN_INFRA_ENTRY( \
		CONN_INFRA_CFG_GALS_CONN2GPS_SLP_CTRL_R_CONN2GPS_SLP_PROT_RX_RDY, \
		val, timeout, p_is_okay)


#define GDL_HW_SET_CONN2GPS_SLP_PROT_TX_VAL(val) \
	GDL_HW_SET_CONN_INFRA_ENTRY( \
		CONN_INFRA_CFG_GALS_CONN2GPS_SLP_CTRL_R_CONN2GPS_SLP_PROT_TX_EN, val)

#define GDL_HW_POLL_CONN2GPS_SLP_PROT_TX_UNTIL_VAL(val, timeout, p_is_okay) \
	GDL_HW_POLL_CONN_INFRA_ENTRY( \
		CONN_INFRA_CFG_GALS_CONN2GPS_SLP_CTRL_R_CONN2GPS_SLP_PROT_TX_RDY, \
		val, timeout, p_is_okay)


#define GDL_HW_SET_GPS2CONN_SLP_PROT_RX_VAL(val) \
	GDL_HW_SET_CONN_INFRA_ENTRY( \
		CONN_INFRA_CFG_GALS_GPS2CONN_SLP_CTRL_R_GPS2CONN_SLP_PROT_RX_EN, val)

#define GDL_HW_POLL_GPS2CONN_SLP_PROT_RX_UNTIL_VAL(val, timeout, p_is_okay) \
	GDL_HW_POLL_CONN_INFRA_ENTRY( \
		CONN_INFRA_CFG_GALS_GPS2CONN_SLP_CTRL_R_GPS2CONN_SLP_PROT_RX_RDY, \
		val, timeout, p_is_okay)


#define GDL_HW_SET_GPS2CONN_SLP_PROT_TX_VAL(val) \
	GDL_HW_SET_CONN_INFRA_ENTRY( \
		CONN_INFRA_CFG_GALS_GPS2CONN_SLP_CTRL_R_GPS2CONN_SLP_PROT_TX_EN, val)

#define GDL_HW_POLL_GPS2CONN_SLP_PROT_TX_UNTIL_VAL(val, timeout, p_is_okay) \
	GDL_HW_POLL_CONN_INFRA_ENTRY( \
		CONN_INFRA_CFG_GALS_GPS2CONN_SLP_CTRL_R_GPS2CONN_SLP_PROT_TX_RDY, \
		val, timeout, p_is_okay)


/* For Connac2.0, wait until sleep prot disable done, or after polling 10 x 1ms */
#define GDL_HW_MAY_WAIT_CONN_INFRA_SLP_PROT_DISABLE_ACK(p_poll_okay) \
	GDL_HW_POLL_CONN_INFRA_ENTRY( \
		CONN_HOST_CSR_TOP_CONN_SLP_PROT_CTRL_CONN_INFRA_ON2OFF_SLP_PROT_ACK, \
		0, 10 * 1000 * POLL_US, p_poll_okay)


/* The dump address list for Connac2.0 */
#define GDL_HW_DUMP_SLP_RPOT_STATUS() do {\
		gps_dl_bus_rd_opt(GPS_DL_CONN_INFRA_BUS, \
			CONN_INFRA_CFG_GALS_CONN2GPS_SLP_CTRL_ADDR, \
			BMASK_RW_FORCE_PRINT); \
		gps_dl_bus_rd_opt(GPS_DL_CONN_INFRA_BUS, \
			CONN_INFRA_CFG_GALS_GPS2CONN_SLP_CTRL_ADDR, \
			BMASK_RW_FORCE_PRINT); \
	} while (0)


#if (GPS_DL_HAS_CONNINFRA_DRV)
/*
 * For MT6885 and MT6893, conninfra driver do it due to GPS/BT share same bit,
 * so do nothing here if conninfra driver ready.
 */
#define GDL_HW_SET_CONN_INFRA_BGF_EN_MT6885_MT6893(val)
#else
#define GDL_HW_SET_CONN_INFRA_BGF_EN_MT6885_MT6893(val) \
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_INFRA_RGU_BGFYS_ON_TOP_PWR_CTL_BGFSYS_ON_TOP_PWR_ON, val)
#endif
#define GDL_HW_SET_CONN_INFRA_BGF_EN(val) GDL_HW_SET_CONN_INFRA_BGF_EN_MT6885_MT6893(val)

#define GDL_HW_SET_GPS_FUNC_EN(val) \
	GDL_HW_SET_CONN_INFRA_ENTRY(CONN_INFRA_CFG_GPS_PWRCTRL0_GP_FUNCTION_EN, val)

#endif /* _GPS_DL_HW_DEP_API_H */

