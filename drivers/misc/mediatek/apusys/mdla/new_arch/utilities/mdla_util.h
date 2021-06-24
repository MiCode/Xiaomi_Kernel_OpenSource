/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_UTIL_H__
#define __MDLA_UTIL_H__

#include <linux/types.h>
#include <linux/platform_device.h>

#include <common/mdla_device.h>

#ifdef RV_COMP
#define apu_device_power_suspend(user, suspend) 0
#define apusys_boost_value_to_opp(user, boost_value) 0
#define trace_async_tag(...)
#endif

/* MDLA command priority level */
enum MDLA_PRIORITY {
	MDLA_LOW_PRIORITY,
	MDLA_HIGH_PRIORITY,

	MDLA_PRIORITY_MAX
};

/* apusys MDLA device priority level */
#if IS_ENABLED(CONFIG_MTK_APUSYS_RT_SUPPORT)
#define PRIORITY_LEVEL      2
#else
#define PRIORITY_LEVEL      1
#endif

/* platform */
const struct of_device_id *mdla_util_get_device_id(void);
u32 mdla_util_get_core_num(void);
u32 mdla_util_get_ip_version(void);

/* return 0 or 1 if success (0: AP version, 1: uP version) */
int mdla_util_plat_init(struct platform_device *pdev);
void mdla_util_plat_deinit(struct platform_device *pdev);

#define for_each_mdla_core(i)\
for (i = 0;  i < mdla_util_get_core_num(); i++)

#define core_id_is_invalid(i) ((i) >= mdla_util_get_core_num())

/* pmu operation */
#define MDLA_PMU_COUNTERS   15
#define COUNTER_CLEAR       (0xFFFFFFFF)

enum CMD_MODE {
	NORMAL,
	PER_CMD,
	INTERRUPT,

	CMD_MODE_MAX,
};

struct mdla_pmu_info;
struct apusys_cmd_hnd;

void mdla_util_pmu_cmd_timer(bool enable);

struct mdla_util_pmu_ops {
	void (*reg_counter_save)(u32 core_id, struct mdla_pmu_info *pmu);
	void (*reg_counter_read)(u32 core_id, u32 *out);

	void (*clr_counter_variable)(struct mdla_pmu_info *pmu);
	void (*clr_cycle_variable)(struct mdla_pmu_info *pmu);
	u32 (*get_num_evt)(u32 core_id, u16 priority);
	void (*set_num_evt)(u32 core_id, u16 priority, u32 val);
	void (*set_percmd_mode)(struct mdla_pmu_info *pmu, u32 mode);
	int (*get_curr_mode)(struct mdla_pmu_info *pmu);
	u32 (*get_perf_end)(struct mdla_pmu_info *pmu);
	u32 (*get_perf_cycle)(struct mdla_pmu_info *pmu);
	u16 (*get_perf_cmdid)(struct mdla_pmu_info *pmu);
	u16 (*get_perf_cmdcnt)(struct mdla_pmu_info *pmu);
	u32 (*get_counter)(struct mdla_pmu_info *pmu, u32 idx);

	void (*reset_counter)(u32 core_id);
	void (*disable_counter)(u32 core_id);
	void (*enable_counter)(u32 core_id);
	void (*reset)(u32 core_id);
	void (*write_evt_exec)(u32 core_id, u16 priority);
	void (*reset_write_evt_exec)(u32 core_id, u16 priority);

	/* apusys pmu hnd */
	u32 (*get_hnd_evt)(struct mdla_pmu_info *pmu, u32 counter_idx);
	u32 (*get_hnd_evt_num)(struct mdla_pmu_info *pmu);
	u32 (*get_hnd_mode)(struct mdla_pmu_info *pmu);
	u64 (*get_hnd_buf_addr)(struct mdla_pmu_info *pmu);

	void (*set_evt_handle)(struct mdla_pmu_info *pmu,
			u32 counter_idx, u32 val);
	struct mdla_pmu_info *(*get_info)(u32 core_id, u16 priority);
	int (*apu_cmd_prepare)(struct mdla_dev *mdla_info,
			struct apusys_cmd_hnd *apusys_hd, u16 priority);
	bool (*apu_pmu_valid)(struct mdla_dev *mdla_info,
			struct apusys_cmd_hnd *apusys_hd, u16 priority);
};

struct mdla_util_pmu_ops *mdla_util_pmu_ops_get(void);

/* apusys pmu operation */
void mdla_util_apusys_pmu_support(bool enable);
int mdla_util_apu_pmu_handle(struct mdla_dev *mdla_info,
	struct apusys_cmd_hnd *apusys_hd, u16 priority);
void mdla_util_apu_pmu_update(struct mdla_dev *mdla_info,
	struct apusys_cmd_hnd *apusys_hd, u16 priority);


/* IO operation */
struct mdla_util_core_io_ops {
	u32 (*read)(u32 id, u32 offset);
	void (*write)(u32 id, u32 offset, u32 value);
	void (*set_b)(u32 id, u32 offset, u32 value);
	void (*clr_b)(u32 id, u32 offset, u32 value);
};

struct mdla_util_common_io_ops {
	u32 (*read)(u32 offset);
	void (*write)(u32 offset, u32 value);
	void (*set_b)(u32 offset, u32 value);
	void (*clr_b)(u32 offset, u32 value);
};

struct mdla_util_io_ops {
	struct mdla_util_core_io_ops cfg;
	struct mdla_util_core_io_ops cmde;
	struct mdla_util_core_io_ops biu;
	struct mdla_util_common_io_ops apu_conn;
	struct mdla_util_common_io_ops infra_cfg;
};

struct mdla_reg_ctl {
	void *apu_mdla_cmde_mreg_top;
	void *apu_mdla_config_top;
	void *apu_mdla_biu_top;
};

enum EXTRA_ADDR_TYPE {
	EXTRA_ADDR_V1P0,
	EXTRA_ADDR_V1PX,
	EXTRA_ADDR_V2P0,
};

const struct mdla_util_io_ops *mdla_util_io_ops_get(void);
void mdla_util_io_set_addr(struct mdla_reg_ctl *reg_ctl);
void mdla_util_io_set_extra_addr(int type,
		void *addr1, void *addr2, void *addr3);

#define dump_reg_cfg(i, name)						\
do {									\
	if (s)								\
		seq_printf(s, "%d: %s: %.8x\n",	i, #name,		\
			mdla_util_io_ops_get()->cfg.read(i, name));	\
	else								\
		mdla_timeout_debug("%d: %s: %.8x\n", i, #name,		\
			mdla_util_io_ops_get()->cfg.read(i, name));	\
} while (0)

#define dump_reg_top(i, name)						\
do {									\
	if (s)								\
		seq_printf(s, "%d: %s: %.8x\n",	i, #name,		\
			mdla_util_io_ops_get()->cmde.read(i, name));	\
	else								\
		mdla_timeout_debug("%d: %s: %.8x\n", i, #name,		\
			mdla_util_io_ops_get()->cmde.read(i, name));	\
} while (0)

#endif /* __MDLA_UTIL_H__ */
