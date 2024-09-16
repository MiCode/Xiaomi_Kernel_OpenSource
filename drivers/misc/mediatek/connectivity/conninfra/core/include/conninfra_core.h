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

#ifndef _CONNINFRA_CORE_H_
#define _CONNINFRA_CORE_H_

#include <linux/semaphore.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/time.h>

#include "osal.h"
#include "msg_thread.h"
#include "conninfra.h"

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#define ENABLE_PRE_CAL_BLOCKING_CHECK	1
#define CHIP_RST_REASON_MAX_LEN			128

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
typedef enum _ENUM_DRV_STS_ {
	DRV_STS_POWER_OFF = 0,	/* initial state */
	DRV_STS_POWER_ON = 1,	/* powered on */
	DRV_STS_RESET = 2,
	DRV_STS_MAX
} ENUM_DRV_STS, *P_ENUM_DRV_STS;

enum pre_cal_status {
	PRE_CAL_NOT_INIT = 0,
	PRE_CAL_NEED_RESCHEDULE = 1,
	PRE_CAL_SCHEDULED = 2,
	PRE_CAL_EXECUTING = 3,
	PRE_CAL_DONE = 4
};

enum pre_cal_caller {
	PRE_CAL_BY_NONE = 0,
	PRE_CAL_BY_SUBDRV_REGISTER = 1,
	PRE_CAL_BY_SUBDRV_PWR_ON = 2,
	PRE_CAL_BY_SCREEN_ON = 3
};

enum chip_rst_status {
	CHIP_RST_NONE = 0,
	CHIP_RST_START = 1,
	CHIP_RST_PRE_CB = 2,
	CHIP_RST_RESET = 3,
	CHIP_RST_POST_CB = 4,
	CHIP_RST_DONE = 5
};

struct subsys_drv_inst {
	ENUM_DRV_STS drv_status;	/* Controlled driver status */
	unsigned int rst_state;
	struct sub_drv_ops_cb ops_cb;
	struct msg_thread_ctx msg_ctx;
};

struct pre_cal_info {
	enum pre_cal_status status;
	enum pre_cal_caller caller;
	struct work_struct pre_cal_work;
	OSAL_SLEEPABLE_LOCK pre_cal_lock;
};


/*
 * state of conninfra
 *
 */
struct conninfra_ctx {
	ENUM_DRV_STS infra_drv_status;

	struct subsys_drv_inst drv_inst[CONNDRV_TYPE_MAX];
	/*struct spinlock infra_lock;*/
	spinlock_t infra_lock;

	OSAL_SLEEPABLE_LOCK core_lock;

	/* chip reset */
	enum chip_rst_status rst_status;
	spinlock_t rst_lock;

	struct semaphore rst_sema;
	atomic_t rst_state;
	enum consys_drv_type trg_drv;
	char trg_reason[CHIP_RST_REASON_MAX_LEN];

	/* pre_cal */
	struct semaphore pre_cal_sema;
	atomic_t pre_cal_state;

	struct msg_thread_ctx msg_ctx;
	struct msg_thread_ctx cb_ctx;

	unsigned int hw_ver;
	unsigned int fw_ver;
	unsigned int ip_ver;

	struct osal_op_history cored_op_history;

	struct pre_cal_info cal_info;

};

//typedef enum _ENUM_CONNINFRA_CORE_OPID_T {
typedef enum {
	CONNINFRA_OPID_PWR_ON 			= 0,
	CONNINFRA_OPID_PWR_OFF			= 1,
	CONNINFRA_OPID_THERM_CTRL		= 2,
	CONNINFRA_OPID_RFSPI_READ		= 5,
	CONNINFRA_OPID_RFSPI_WRITE		= 6,
	CONNINFRA_OPID_ADIE_TOP_CK_EN_ON	= 7,
	CONNINFRA_OPID_ADIE_TOP_CK_EN_OFF	= 8,
	CONNINFRA_OPID_SPI_CLOCK_SWITCH		= 9,
	CONNINFRA_OPID_CLOCK_FAIL_DUMP		= 10,
	CONNINFRA_OPID_PRE_CAL_PREPARE		= 11,
	CONNINFRA_OPID_PRE_CAL_CHECK		= 12,
	CONNINFRA_OPID_FORCE_CONNINFRA_WAKUP	= 13,
	CONNINFRA_OPID_FORCE_CONNINFRA_SLEEP	= 14,
	CONNINFRA_OPID_DUMP_POWER_STATE			= 15,
	CONNINFRA_OPID_MAX
} conninfra_core_opid;

/* For the operation which may callback subsys driver */
typedef enum {
	CONNINFRA_CB_OPID_CHIP_RST         = 0,
	CONNINFRA_CB_OPID_PRE_CAL          = 1,
	CONNINFRA_CB_OPID_MAX
} conninfra_core_cb_opid;

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

extern int conninfra_core_init(void);
extern int conninfra_core_deinit(void);

int conninfra_core_power_on(enum consys_drv_type type);
int conninfra_core_power_off(enum consys_drv_type type);

int conninfra_core_lock_rst(void);
int conninfra_core_unlock_rst(void);
int conninfra_core_trg_chip_rst(enum consys_drv_type drv, char *reason);

int conninfra_core_subsys_ops_reg(enum consys_drv_type type,
						struct sub_drv_ops_cb *cb);
int conninfra_core_subsys_ops_unreg(enum consys_drv_type type);


int conninfra_core_screen_on(void);
int conninfra_core_screen_off(void);

/* pre_cal */
int conninfra_core_pre_cal_start(void);
#if ENABLE_PRE_CAL_BLOCKING_CHECK
void conninfra_core_pre_cal_blocking(void);
#endif

/*       reg control      */
/* NOTE: NOT thread-safe
 * return value
 * 1 : Yes, 0: NO
 */
int conninfra_core_reg_readable(void);
int conninfra_core_reg_readable_no_lock(void);
int conninfra_core_is_bus_hang(void);

int conninfra_core_is_consys_reg(phys_addr_t addr);
int conninfra_core_reg_read(unsigned long address, unsigned int *value, unsigned int mask);
int conninfra_core_reg_write(unsigned long address, unsigned int value, unsigned int mask);

int conninfra_core_is_rst_locking(void);

int conninfra_core_thermal_query(int *temp_val);
void conninfra_core_clock_fail_dump_cb(void);

int conninfra_core_spi_read(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int *data);
int conninfra_core_spi_write(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int data);

int conninfra_core_adie_top_ck_en_on(enum consys_adie_ctl_type type);
int conninfra_core_adie_top_ck_en_off(enum consys_adie_ctl_type type);

int conninfra_core_force_conninfra_wakeup(void);
int conninfra_core_force_conninfra_sleep(void);

int conninfra_core_spi_clock_switch(enum connsys_spi_speed_type type);

int conninfra_core_dump_power_state(void);
int conninfra_core_pmic_event_cb(unsigned int, unsigned int);

void conninfra_core_config_setup(void);

int conninfra_core_bus_clock_ctrl(enum consys_drv_type drv_type, unsigned int bus_clock, int status);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif				/* _CONNINFRA_CORE_H_ */
