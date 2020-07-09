/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_DCM_COMMON_H__
#define __MTK_DCM_COMMON_H__

#include <linux/ratelimit.h>

#define DCM_OFF (0)
#define DCM_ON (1)
#define DCM_DEFAULT (-1)

#define TAG	"[Power/dcm] "
#define dcm_pr_notice(fmt, args...)			\
	pr_notice(TAG fmt, ##args)
#define dcm_pr_info_limit(fmt, args...)			\
	pr_info_ratelimited(TAG fmt, ##args)
#define dcm_pr_info(fmt, args...)			\
	pr_info(TAG fmt, ##args)
#define dcm_pr_dbg(fmt, args...)			\
	do {						\
		if (dcm_debug)				\
			pr_info(TAG fmt, ##args);	\
	} while (0)

/** macro **/
#define and(v, a) ((v) & (a))
#define or(v, o) ((v) | (o))
#define aor(v, a, o) (((v) & (a)) | (o))

#define DCM_BASE_INFO(_name) \
{ \
	.name = #_name, \
	.base = &_name, \
}
/**/
enum {
	ARMCORE_DCM_OFF = DCM_OFF,
	ARMCORE_DCM_MODE1 = DCM_ON,
	ARMCORE_DCM_MODE2 = DCM_ON+1,
};

enum {
	INFRA_DCM_OFF = DCM_OFF,
	INFRA_DCM_ON = DCM_ON,
};

enum {
	PERI_DCM_OFF = DCM_OFF,
	PERI_DCM_ON = DCM_ON,
};

enum {
	MCUSYS_DCM_OFF = DCM_OFF,
	MCUSYS_DCM_ON = DCM_ON,
};

enum {
	DRAMC_AO_DCM_OFF = DCM_OFF,
	DRAMC_AO_DCM_ON = DCM_ON,
};

enum {
	DDRPHY_DCM_OFF = DCM_OFF,
	DDRPHY_DCM_ON = DCM_ON,
};

enum {
	EMI_DCM_OFF = DCM_OFF,
	EMI_DCM_ON = DCM_ON,
};

enum {
	STALL_DCM_OFF = DCM_OFF,
	STALL_DCM_ON = DCM_ON,
};

enum {
	BIG_CORE_DCM_OFF = DCM_OFF,
	BIG_CORE_DCM_ON = DCM_ON,
};

enum {
	GIC_SYNC_DCM_OFF = DCM_OFF,
	GIC_SYNC_DCM_ON = DCM_ON,
};

enum {
	LAST_CORE_DCM_OFF = DCM_OFF,
	LAST_CORE_DCM_ON = DCM_ON,
};

enum {
	RGU_DCM_OFF = DCM_OFF,
	RGU_DCM_ON = DCM_ON,
};

enum {
	TOPCKG_DCM_OFF = DCM_OFF,
	TOPCKG_DCM_ON = DCM_ON,
};

enum {
	LPDMA_DCM_OFF = DCM_OFF,
	LPDMA_DCM_ON = DCM_ON,
};

enum {
	PWRAP_DCM_OFF = DCM_OFF,
	PWRAP_DCM_ON = DCM_ON,
};

enum {
	MCSI_DCM_OFF = DCM_OFF,
	MCSI_DCM_ON = DCM_ON,
};

enum {
	ARMCORE_DCM = 0,
	MCUSYS_DCM,
	INFRA_DCM,
	PERI_DCM,
	EMI_DCM,
	DRAMC_DCM,
	DDRPHY_DCM,
	STALL_DCM,
	BIG_CORE_DCM,
	GIC_SYNC_DCM,
	LAST_CORE_DCM,
	RGU_DCM,
	TOPCKG_DCM,
	LPDMA_DCM,
	MCSI_DCM,
	NR_DCM,
};

enum {
	ARMCORE_DCM_TYPE	= (1U << ARMCORE_DCM),
	MCUSYS_DCM_TYPE		= (1U << MCUSYS_DCM),
	INFRA_DCM_TYPE		= (1U << INFRA_DCM),
	PERI_DCM_TYPE		= (1U << PERI_DCM),
	EMI_DCM_TYPE		= (1U << EMI_DCM),
	DRAMC_DCM_TYPE		= (1U << DRAMC_DCM),
	DDRPHY_DCM_TYPE		= (1U << DDRPHY_DCM),
	STALL_DCM_TYPE		= (1U << STALL_DCM),
	BIG_CORE_DCM_TYPE	= (1U << BIG_CORE_DCM),
	GIC_SYNC_DCM_TYPE	= (1U << GIC_SYNC_DCM),
	LAST_CORE_DCM_TYPE	= (1U << LAST_CORE_DCM),
	RGU_DCM_TYPE		= (1U << RGU_DCM),
	TOPCKG_DCM_TYPE		= (1U << TOPCKG_DCM),
	LPDMA_DCM_TYPE		= (1U << LPDMA_DCM),
	MCSI_DCM_TYPE		= (1U << MCSI_DCM),
	NR_DCM_TYPE = NR_DCM,
};

enum {
	DCM_CPU_CLUSTER_LL	= (1U << 0),
	DCM_CPU_CLUSTER_L	= (1U << 1),
	DCM_CPU_CLUSTER_B	= (1U << 2),
};

/*****************************************************/
typedef int (*DCM_FUNC)(int);
typedef void (*DCM_FUNC_VOID_VOID)(void);
typedef void (*DCM_FUNC_VOID_UINTR)(unsigned int *);
typedef void (*DCM_FUNC_VOID_UINTR_INTR)(unsigned int *, int *);
typedef void (*DCM_PRESET_FUNC)(void);
typedef void (*DCM_FUNC_VOID_UINT)(unsigned int);

struct DCM_OPS {
	DCM_FUNC_VOID_VOID dump_regs;
	DCM_FUNC_VOID_UINTR_INTR get_default;
	DCM_FUNC_VOID_UINTR get_init_type;
	DCM_FUNC_VOID_UINTR get_all_type;
	DCM_FUNC_VOID_UINTR get_init_by_k_type;
	DCM_FUNC_VOID_UINT set_debug_mode;
};

struct DCM_BASE {
	char *name;
	unsigned long *base;
};

struct DCM {
	int current_state;
	int saved_state;
	int disable_refcnt;
	int default_state;
	DCM_FUNC func;
	DCM_PRESET_FUNC preset_func;
	int typeid;
	char *name;
};

/*extern short dcm_debug;*/
/*extern short dcm_initiated;*/
/*extern unsigned int all_dcm_type;*/
/*extern unsigned int init_dcm_type;*/
/*extern struct mutex dcm_lock;*/

/*void dcm_dump_regs(void);*/
/*int dcm_smc_get_cnt(int type_id);*/
/*void dcm_smc_msg_send(unsigned int msg);*/
/*short is_dcm_bringup(void);*/

#endif /* #ifndef __MTK_DCM_COMMON_H__ */

