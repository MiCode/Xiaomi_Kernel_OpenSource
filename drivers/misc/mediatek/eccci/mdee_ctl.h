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

#ifndef __MDEE_CTL_H__
#define __MDEE_CTL_H__

#include "ccci_core.h"

#define CCCI_AED_DUMP_EX_MEM		(1<<0)
#define CCCI_AED_DUMP_MD_IMG_MEM	(1<<1)
#define CCCI_AED_DUMP_CCIF_REG		(1<<2)
#define CCCI_AED_DUMP_EX_PKT		(1<<3)
#define MD_EX_MPU_STR_LEN		(128)
enum {
	MD_EE_FLOW_START	= (1 << 0),
	MD_EE_DUMP_ON_GOING = (1 << 1),
	MD_STATE_UPDATE		= (1 << 2),
	MD_EE_MSG_GET		= (1 << 3),
	MD_EE_TIME_OUT_SET	= (1 << 4),
	MD_EE_OK_MSG_GET	= (1 << 5),
	MD_EE_PENDING_TOO_LONG	= (1 << 6),
	MD_EE_SWINT_GET			= (1 << 7),
	MD_EE_WDT_GET			= (1 << 8),
	MD_EE_PASS_MSG_GET		= (1 << 9),
	MD_EE_TIMER1_DUMP_ON_GOING	= (1 << 11),
	MD_EE_TIMER2_DUMP_ON_GOING	= (1 << 12),
};

enum {
	MD_EE_CASE_NORMAL = 0,
	MD_EE_CASE_ONLY_EX,
	MD_EE_CASE_ONLY_EX_OK,
	MD_EE_CASE_TX_TRG,	/* not using */
	MD_EE_CASE_ISR_TRG,	/* not using */
	MD_EE_CASE_NO_RESPONSE,
	MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG,	/* not using */
	MD_EE_CASE_ONLY_SWINT,
	MD_EE_CASE_SWINT_MISSING,
	MD_EE_CASE_WDT,
};

typedef enum {
	MDEE_DUMP_LEVEL_BOOT_FAIL,
	MDEE_DUMP_LEVEL_TIMER1,
	MDEE_DUMP_LEVEL_TIMER2,
} MDEE_DUMP_LEVEL;

struct md_ee;
struct md_ee_ops {
	 void (*set_ee_pkg)(struct md_ee *mdee, char *data, int len);
	 void (*dump_ee_info)(struct md_ee *mdee, MDEE_DUMP_LEVEL level, int more_info);
};
struct md_ee {
	int md_id;
	MD_EX_STAGE ex_stage;	/* only for logging */
	unsigned int ee_info_flag;
	void *md_obj;
	spinlock_t ctrl_lock;
	unsigned int ee_case;
	unsigned int ex_type;
	void *dumper_obj;
	struct md_ee_ops *ops;
	char ex_mpu_string[MD_EX_MPU_STR_LEN];
};
/****************************************************************************************************************/
/* API Region called by ccci modem object */
/****************************************************************************************************************/
void mdee_state_notify(struct md_ee *mdee, MD_EX_STAGE stage);
struct md_ee *mdee_alloc(int md_id, void *md_obj);
static inline int mdee_flow_is_start(struct md_ee *mdee)
{
	return mdee->ee_info_flag & MD_EE_FLOW_START;
}
static inline int mdee_get_ex_stage(struct md_ee *mdee)
{
	return mdee->ex_stage;
}
static inline void mdee_set_ex_mpu_str(struct md_ee *mdee, char *str)
{
	snprintf(mdee->ex_mpu_string, MD_EX_MPU_STR_LEN, "EMI MPU VIOLATION: %s", str);
}

unsigned int mdee_get_ee_type(struct md_ee *mdee);
void mdee_monitor_func(struct md_ee *mdee);
void mdee_monitor2_func(struct md_ee *mdee);

/****************************************************************************************************************/
/* API Region called by mdee object */
/****************************************************************************************************************/
extern int mdee_dumper_v1_alloc(struct md_ee *mdee);
extern int mdee_dumper_v2_alloc(struct md_ee *mdee);
#endif	/* __MDEE_CTL_H__ */
