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

/****************************************************************************
 *  Macro Definitions
 ****************************************************************************/
#define IDD_TBL_DBG

#define MAX(a, b)			((a) >= (b) ? (a) : (b))
#define MIN(a, b)			((a) >= (b) ? (b) : (a))

#define SWPM_OPS (swpm_m.plat_ops)

#define swpm_lock(lock)		mutex_lock(lock)
#define swpm_unlock(lock)	mutex_unlock(lock)

#if 0
#define SWPM_TAG     "[SWPM] "
#define swpm_err                swpm_info
#define swpm_warn               swpm_info
#define swpm_info(fmt, args...) pr_notice(SWPM_TAGfmt, ##args)
#endif

#define swpm_get_status(type)  ((swpm_status & (1 << type)) >> type)
#define swpm_set_status(type)  (swpm_status |= (1 << type))
#define swpm_clr_status(type)  (swpm_status &= ~(1 << type))

#define DEFAULT_LOG_INTERVAL_MS		(1000)
#define MAX_IP_NAME_LENGTH (16)
/* SWPM command dispatcher with user bits */
#define SWPM_CODE_USER_BIT (16)
/****************************************************************************
 *  Type Definitions
 ****************************************************************************/
enum swpm_return_type {
	SWPM_SUCCESS = 0,
	SWPM_INIT_ERR = 1,
	SWPM_PLAT_ERR = 2,
	SWPM_ARGS_ERR = 3,
};

enum swpm_type {
	CPU_SWPM_TYPE,
	GPU_SWPM_TYPE,
	CORE_SWPM_TYPE,
	MEM_SWPM_TYPE,
	ISP_SWPM_TYPE,
	ME_SWPM_TYPE,

	NR_SWPM_TYPE,
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
	SYNC_DATA,
	SET_INTERVAL,
	SET_PMU,
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
extern int swpm_mem_addr_request(enum swpm_type id,
				 phys_addr_t **ptr);


/* swpm interface to enable/disable swpm related pmu */
/* return:	0  (SWPM_SUCCESS)
 *		otherwise (ERROR)
 */
extern int swpm_pmu_enable(enum swpm_pmu_user id,
			   unsigned int enable);

extern int swpm_reserve_mem_init(phys_addr_t *virt,
				 unsigned long long *size);

#endif
