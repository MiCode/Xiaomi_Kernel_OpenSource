/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _PLATFORM_MT6893_CONSYS_REG_H_
#define _PLATFORM_MT6893_CONSYS_REG_H_

#include "consys_reg_base.h"
/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

enum consys_base_addr_index {
	CONN_INFRA_RGU_BASE_INDEX = 0,
	CONN_INFRA_CFG_BASE_INDEX = 1,
	CONN_HOST_CSR_TOP_BASE_INDEX = 2,
	INFRACFG_AO_BASE_INDEX = 3,
	TOPRGU_BASE_INDEX= 4,
	SPM_BASE_INDEX = 5,
	INFRACFG_BASE_INDEX = 6,
	CONN_WT_SLP_CTL_REG_INDEX = 7,
	CONN_AFE_CTL_INDEX = 8,
	CONN_INFRA_SYSRAM_INDEX = 9,
	GPIO_INDEX = 10,
	CONN_RF_SPI_MST_REG_INDEX = 11,
	CONN_SEMAPHORE_INDEX = 12,
	CONN_TOP_THERM_CTL_INDEX = 13,
	IOCFG_RT_INDEX = 14, /* Base: 0x11EA_0000 */
	CONN_DEBUG_CTRL = 15, /* Base: 0x1800_f000 */

	CONSYS_BASE_ADDR_MAX
};


struct consys_base_addr {
	struct consys_reg_base_addr reg_base_addr[CONSYS_BASE_ADDR_MAX];
};

extern struct consys_base_addr conn_reg;

#define CON_REG_INFRA_RGU_ADDR 		conn_reg.reg_base_addr[CONN_INFRA_RGU_BASE_INDEX].vir_addr
#define CON_REG_INFRA_CFG_ADDR 		conn_reg.reg_base_addr[CONN_INFRA_CFG_BASE_INDEX].vir_addr
#define CON_REG_HOST_CSR_ADDR 		conn_reg.reg_base_addr[CONN_HOST_CSR_TOP_BASE_INDEX].vir_addr
#define CON_REG_INFRACFG_AO_ADDR 	conn_reg.reg_base_addr[INFRACFG_AO_BASE_INDEX].vir_addr

#define CON_REG_TOP_RGU_ADDR 		conn_reg.reg_base_addr[TOPRGU_BASE_INDEX].vir_addr
#define CON_REG_SPM_BASE_ADDR 		conn_reg.reg_base_addr[SPM_BASE_INDEX].vir_addr
#define CON_REG_INFRACFG_BASE_ADDR 	conn_reg.reg_base_addr[INFRACFG_BASE_INDEX].vir_addr
#define CON_REG_WT_SPL_CTL_ADDR 	conn_reg.reg_base_addr[CONN_WT_SLP_CTL_REG_INDEX].vir_addr

#define CONN_AFE_CTL_BASE_ADDR		conn_reg.reg_base_addr[CONN_AFE_CTL_INDEX].vir_addr
#define CONN_INFRA_SYSRAM_BASE_ADDR	conn_reg.reg_base_addr[CONN_INFRA_SYSRAM_INDEX].vir_addr
#define GPIO_BASE_ADDR			conn_reg.reg_base_addr[GPIO_INDEX].vir_addr
#define CONN_REG_RFSPI_ADDR		conn_reg.reg_base_addr[CONN_RF_SPI_MST_REG_INDEX].vir_addr

#define CONN_REG_SEMAPHORE_ADDR		conn_reg.reg_base_addr[CONN_SEMAPHORE_INDEX].vir_addr
#define CONN_TOP_THERM_CTL_ADDR		conn_reg.reg_base_addr[CONN_TOP_THERM_CTL_INDEX].vir_addr
#define IOCFG_RT_ADDR			conn_reg.reg_base_addr[IOCFG_RT_INDEX].vir_addr
#define CONN_DEBUG_CTRL_ADDR		conn_reg.reg_base_addr[CONN_DEBUG_CTRL].vir_addr

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/


/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

struct consys_base_addr* get_conn_reg_base_addr(void);

#endif				/* _PLATFORM_MT6893_CONSYS_REG_H_ */
