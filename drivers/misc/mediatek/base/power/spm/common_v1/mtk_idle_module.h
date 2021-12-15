/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_IDLE_MODULE_H__
#define __MTK_IDLE_MODULE_H__

#include <linux/notifier.h>

#include <mtk_idle.h>
#include <mtk_idle_internal.h>

#define IDLE_TYPE_LEGACY_ENTER			IDLE_TYPE_SO
#define IDLE_TYPE_LEGACY_CAN_NOT_ENTER	IDLE_TYPE_RG

#define MTK_IDLE_MODULE_MODEL_MAX	12
#define MTK_IDLE_MODEL_NAME_MAX		16

#define	MTK_IDLE_MOD_OK				(-1)
#define	MTK_IDLE_MOD_UNKNOWN		(-2)
#define	MTK_IDLE_MOD_ERR_PARAM		(-3)
#define	MTK_IDLE_MOD_ERROR			(-4)
#define	MTK_IDLE_MOD_FAIL			(-5)
#define	MTK_IDLE_MOD_NOT_SUPPORT	(-6)

#define MTK_IDLE_MOD_SUCCESS(x)\
	(x == MTK_IDLE_MOD_OK)


/* mtk idle notify definition - START */
#define	MTK_IDLE_MAINPLL_OFF	(1<<0u)
#define	MTK_IDLE_MAINPLL_ON		(1<<1u)
#define	MTK_IDLE_26M_OFF		(1<<2u)
#define	MTK_IDLE_26M_ON			(1<<3u)
/* mtk idle notify definition - END */

enum MTK_IDLE_MODULE_INFO_TYPE {
	MTK_IDLE_MODULE_INFO_COUNT,
	MTK_IDLE_MODULE_INFO_RATIO,
	MTK_IDLE_MODULE_INFO_IDLE_STATE,
	MTK_IDLE_MODULE_INFO_IDLE_ENABLED,
};

enum MTK_IDLE_MODULE_FEATURE {
	MTK_IDLE_MODULE_RATIO_CAL,
};

enum MTK_IDLE_MODEL_CLR_BIT {
	MTK_IDLE_MODEL_CLR_RESIDENCY,
};

struct MTK_IDLE_MODEL_LATENCY {
	/* Values verify in mtk_idle_latency_profile_result */
	unsigned long total[3];
	unsigned int count;
};

struct MTK_IDLE_MODEL_COUNTER {
	/* enter will be verify in mtk_idle_module_model_enter() */
	unsigned long enter[NR_CPUS];
	unsigned long prev_enter[NR_CPUS];

	/* block will be verify in mtk_idle_select_state() */
	unsigned long block[NR_REASONS];
};

struct MTK_IDLE_MODEL_STATUS {
	struct MTK_IDLE_MODEL_LATENCY latency;
	struct MTK_IDLE_MODEL_COUNTER cnt;
	unsigned long long residency_ms;
	unsigned int clr;
	u64 prev_time;
};

struct MTK_IDLE_MODEL_INFO {
	char *name;
	int enable;
	struct MTK_IDLE_MODEL_COUNTER cnt;
};

struct MTK_IDLE_MODULE_MODLES_INFO {
	int num;
	struct MTK_IDLE_MODEL_INFO mod[MTK_IDLE_MODULE_MODEL_MAX];
};

#define IS_MTK_IDLE_VALID_STATE(x, _type)\
	(((x>>_type) > 0) && (((x>>_type)&0x1) > 0))

#define mtk_idle_name(x)\
	mtk_idle_module_get_mod_name(x)

#define MTK_IDLE_MODEL_STATUS_INIT(x) \
	memset(&x, 0, sizeof(struct MTK_IDLE_MODEL_STATUS))


struct MTK_IDLE_MODEL_NOTIFY {
	unsigned int id_enter;
	unsigned int id_leave;
};

struct MTK_IDLE_MODEL_CLERK {
	char *name;
	int type;
	u32 time_critera;
	struct MTK_IDLE_MODEL_STATUS status;
};

struct MTK_IDLE_MODEL_NOTE {
	int id;
	union {
		int VariableInt;
	} value;
};

struct MTK_IDLE_MODEL_POLICY_FUNC {
	void (*init)(struct mtk_idle_init_data *data);

	int (*enter)(int cpu);

	bool (*can_enter)(int cpu, struct mtk_idle_info *info);

	bool (*enabled)(void);

	int (*receiver)(struct MTK_IDLE_MODEL_NOTE *note);

	void (*deinit)(void);
};

struct MTK_IDLE_MODEL {
	struct MTK_IDLE_MODEL_CLERK clerk;
	struct MTK_IDLE_MODEL_NOTIFY notify;
	struct MTK_IDLE_MODEL_POLICY_FUNC policy;
};

struct MTK_IDLE_MODULE_INIT {
	int (*attach)(void);
	int (*dettach)(void);
};

struct MTK_IDLE_MODULE_REGISTRY {
	int (*get_init_data)(struct mtk_idle_init_data *data);
	size_t (*get_helper_info)(char *buf, size_t sz);
	struct MTK_IDLE_MODULE_INIT init;
};

struct MTK_IDLE_MODULE_FUNC {
	int (*model_switch)(int ModuleType);
	size_t (*model_switch_support)(char *buf, size_t len);
};

struct MTK_IDLE_MODULE {
	struct MTK_IDLE_MODULE_REGISTRY reg;
	struct MTK_IDLE_MODULE_FUNC func;
	struct MTK_IDLE_MODEL **models;
};

int mtk_idle_module_enabled(void);

/* Platform implementation*/
const char *mtk_idle_module_get_mod_name(int IdleModelType);

int mtk_idle_module_model_sel(struct mtk_idle_info *info
			, int reason, int *IdleModelType);


int mtk_idle_module_register(struct MTK_IDLE_MODULE *module);

int mtk_idle_module_unregister(struct MTK_IDLE_MODULE *module);

int mtk_idle_module_model_enter(int IdleModelType, int cpuId);

int mtk_idle_module_enter(struct mtk_idle_info *info
		, int reason, int *IdleModelType);

int mtk_idle_model_count_get(int idle_type
				, struct MTK_IDLE_MODEL_COUNTER *cnt);

int mtk_idle_module_switch(int ModuleType);

size_t mtk_idle_module_switch_support(char *buf, size_t len);

/* return value: the buffer have been filled */
int mtk_idle_module_info_dump(int info_type, char *buf, size_t len);
int mtk_idle_module_info_dump_locked(
			int info_type, char *buf, size_t len);

int mtk_idle_module_feature(int feature, int enabled);

int mtk_idle_model_notify(int IdleModelType, struct MTK_IDLE_MODEL_NOTE *note);
/* */
int mtk_idle_enter(
	int idle_type, int cpu, unsigned int op_cond, unsigned int idle_flag);

bool mtk_idle_select_state(int type, int reason);

int mtk_idle_module_notifier_call_chain(unsigned long notifyID);

int mtk_idle_module_notifier_register(struct notifier_block *n);

int mtk_idle_module_notifier_unregister(struct notifier_block *n);

size_t mtk_idle_module_get_helper(char *buf, size_t sz);

/* Legacy ratio counter */
struct mtk_idle_ratio_recent_info {
	union {
		struct mtk_idle_recent_ratio scenario;
	} ratio;
};
int mtk_idle_mod_recent_ratio_get_plat(
	struct mtk_idle_ratio_recent_info *feed, int *win_ms);

#endif
