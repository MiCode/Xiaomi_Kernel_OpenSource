/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */


#ifndef __SWPM_MODULE_H__
#define __SWPM_MODULE_H__

#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/workqueue.h>

/****************************************************************************
 *  Macro Definitions
 ****************************************************************************/
#define IDD_TBL_DBG

#define MAX(a, b)			((a) >= (b) ? (a) : (b))
#define MIN(a, b)			((a) >= (b) ? (b) : (a))

#define SWPM_OPS (swpm_m.plat_ops)

#define swpm_lock(lock)		mutex_lock(lock)
#define swpm_unlock(lock)	mutex_unlock(lock)

#define swpm_get_status(type)  ((swpm_status & (1 << type)) >> type)
#define swpm_set_status(type)  (swpm_status |= (1 << type))
#define swpm_clr_status(type)  (swpm_status &= ~(1 << type))

#define DEFAULT_LOG_INTERVAL_MS		(1000)
#define MAX_IP_NAME_LENGTH (16)
/****************************************************************************
 *  Type Definitions
 ****************************************************************************/
enum swpm_return_type {
	SWPM_SUCCESS = 0,
	SWPM_INIT_ERR = 1,
	SWPM_PLAT_ERR = 2,
	SWPM_ARGS_ERR = 3,
};

/* SWPM command dispatcher with user bits */
#define SWPM_CODE_USER_BIT (16)
enum swpm_cmd_cfg_code {
	PMSR_CFG_CODE = 0x9696,
};

enum swpm_pmu_user {
	SWPM_PMU_CPU_DVFS,
	SWPM_PMU_INTERNAL,

	NR_SWPM_PMU_USER,
};

struct swpm_mem_ref_tbl {
	bool valid;
	phys_addr_t *virt;
};

enum swpm_cmd_type {
	SWPM_COMMON_INIT,
	SMAP_CMD_TYPE,
	CPU_CMD_TYPE,
	MEM_CMD_TYPE,
	CORE_CMD_TYPE,

	NR_SWPM_CMD_TYPE,
};

enum swpm_ext_cmd_type {
	SYNC_DATA,
	SET_INTERVAL,
	SET_PMU,

	NR_SWPM_EXT_CMD_TYPE,
};

enum swpm_notifier_event {
	SWPM_LOG_DATA_NOTIFY,

	NR_SWPM_NOTIFIER_EVENT,
};

struct swpm_core_internal_ops {
	void (*const cmd)(unsigned int type,
			  unsigned int val);
};

struct swpm_manager {
	bool initialize;
	bool plat_ready;
	struct swpm_mem_ref_tbl *mem_ref_tbl;
	unsigned int ref_tbl_size;
	struct swpm_core_internal_ops *plat_ops;
};

extern struct mutex swpm_mutex;
extern struct timer_list swpm_timer;
extern struct workqueue_struct *swpm_common_wq;
extern unsigned int swpm_log_interval_ms;

extern int swpm_core_ops_register(struct swpm_core_internal_ops *ops);
extern void swpm_get_rec_addr(phys_addr_t *phys,
			      phys_addr_t *virt,
			      unsigned long long *size);
extern int swpm_interface_manager_init(struct swpm_mem_ref_tbl *ref_tbl,
				       unsigned int tbl_size);
/* swpm interface to request share memory address by SWPM TYPE */
/* return:      0  (SWPM_SUCCESS)
 *              otherwise (ERROR)
 */
extern int swpm_mem_addr_request(unsigned int id,
				 phys_addr_t **ptr);


/* swpm interface to enable/disable swpm related pmu */
/* return:	0  (SWPM_SUCCESS)
 *		otherwise (ERROR)
 */
extern int swpm_pmu_enable(enum swpm_pmu_user id,
			   unsigned int enable);

extern int swpm_reserve_mem_init(phys_addr_t *virt,
				 unsigned long long *size);

extern void swpm_set_cmd(unsigned int type, unsigned int cnt);
extern unsigned int swpm_set_and_get_cmd(unsigned int args_0,
					 unsigned int args_1,
					 unsigned int args_2);

extern int swpm_register_event_notifier(struct notifier_block *nb);
extern int swpm_unregister_event_notifier(struct notifier_block *nb);
extern int swpm_call_event_notifier(unsigned long val, void *v);

#endif
