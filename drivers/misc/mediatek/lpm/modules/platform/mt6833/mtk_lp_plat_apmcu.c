// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/cpu.h>
#include <linux/pm_qos.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/time.h>

#include <mtk_lpm.h>

#include <mtk_lp_plat_reg.h>
#include <mtk_lp_plat_apmcu.h>
#include <mtk_lp_plat_apmcu_mbox.h>

void __iomem *cpu_pm_syssram_base;

#define plat_node_ready()       (cpu_pm_syssram_base)

#define BOOT_TIME_LIMIT         60

static struct task_struct *mtk_lp_plat_task;

/* qos */
static struct pm_qos_request mtk_lp_plat_qos_req;

#define mtk_lp_plat_qos_init()\
	pm_qos_add_request(&mtk_lp_plat_qos_req,\
		PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE)
#define mtk_cpu_off_block()\
	pm_qos_update_request(&mtk_lp_plat_qos_req, 2)
#define mtk_cpu_off_allow()\
	pm_qos_update_request(&mtk_lp_plat_qos_req, PM_QOS_DEFAULT_VALUE)
#define mtk_lp_plat_qos_uninit()\
	pm_qos_remove_request(&mtk_lp_plat_qos_req)

#define CHK(cond) WARN_ON(cond)

#define PWR_LVL_BIT(v)  BIT(4 * v)

#define PWR_LVL_MASK(v) (0xF << (4 * (v)))

#define PWR_LVL0        (PWR_LVL_BIT(0))
#define PWR_LVL1        (PWR_LVL_BIT(1) | PWR_LVL0)
#define PWR_LVL2        (PWR_LVL_BIT(2) | PWR_LVL1)
#define PWR_LVL3        (PWR_LVL_BIT(3) | PWR_LVL2)

#define CORE_LVL	PWR_LVL0
#define CLUSTER_LVL	PWR_LVL1
#define MCUSYS_LVL	PWR_LVL2
#define BUS26M_LVL	PWR_LVL3

#define MAX_PWR_LVL     MCUSYS_LVL

struct mtk_lp_device {
	unsigned int pwr_lvl_mask;
	union {
		struct {
			u32 core:4;
			u32 cluster:4;
			u32 mcusys:4;
			u32 bus26m:4;
		};
		u32 lvl;
	} pwr_on;
	struct mtk_lp_device *parent;
};

static struct mtk_lp_device lp_dev_cpu[NR_CPUS];
static struct mtk_lp_device lp_dev_cluster[nr_cluster_ids];
static struct mtk_lp_device lp_dev_mcusys;

static void _mtk_lp_plat_inc_pwr_cnt(struct mtk_lp_device *dev,
					unsigned int lvl)
{
	unsigned int lvl_cnt;

	lvl_cnt = lvl & dev->pwr_lvl_mask;

	if (!lvl_cnt)
		return;

	/**
	 * Exclusive read-modify-write is ensured by
	 *     mtk_lp_mod_locker in idle flow
	 *     mtk_lp_plat_qos_req in hot-plug flow
	 */
	dev->pwr_on.lvl += lvl_cnt;

	CHK(dev->pwr_on.core >= nr_cpu_ids);

	if (dev->parent)
		_mtk_lp_plat_inc_pwr_cnt(dev->parent, lvl);
}

static void _mtk_lp_plat_dec_pwr_cnt(struct mtk_lp_device *dev,
					unsigned int lvl)
{
	unsigned int lvl_cnt;

	lvl_cnt = lvl & dev->pwr_lvl_mask;

	if (!lvl_cnt)
		return;

	/**
	 * Exclusive read-modify-write is ensured by
	 *     mtk_lp_mod_locker in idle flow
	 *     mtk_lp_plat_qos_req in hot-plug flow
	 */
	dev->pwr_on.lvl -= lvl_cnt;

	CHK((dev->pwr_on.lvl & BIT(31)));

	if (dev->parent)
		_mtk_lp_plat_dec_pwr_cnt(dev->parent, lvl);
}

#define mtk_lp_plat_set_off_lvl(cpu, lvl)				\
do {									\
	if (likely(cpu < nr_cpu_ids))					\
		_mtk_lp_plat_dec_pwr_cnt(&lp_dev_cpu[cpu], lvl);	\
} while (0)

#define mtk_lp_plat_clr_off_lvl(cpu, lvl)				\
do {									\
	if (likely(cpu < nr_cpu_ids))					\
		_mtk_lp_plat_inc_pwr_cnt(&lp_dev_cpu[cpu], lvl);	\
} while (0)

void mtk_lp_plat_set_mcusys_off(int cpu)
{
	mtk_lp_plat_set_off_lvl(cpu, MCUSYS_LVL);
}

void mtk_lp_plat_clr_mcusys_off(int cpu)
{
	mtk_lp_plat_clr_off_lvl(cpu, MCUSYS_LVL);
}

void mtk_lp_plat_set_cluster_off(int cpu)
{
	mtk_lp_plat_set_off_lvl(cpu, CLUSTER_LVL);
}

void mtk_lp_plat_clr_cluster_off(int cpu)
{
	mtk_lp_plat_clr_off_lvl(cpu, CLUSTER_LVL);
}

static inline void mtk_lp_dev_get_cpus_online(void)
{
	int i;

	for_each_online_cpu(i)
		mtk_lp_plat_clr_off_lvl(i, MAX_PWR_LVL);
}

static void mtk_lp_dev_set_cpus_off(void)
{
	int i;

	lp_dev_mcusys.pwr_on.lvl = 0;

	for (i = 0; i < nr_cluster_ids; i++)
		lp_dev_cluster[i].pwr_on.lvl = 0;

	for (i = 0; i < nr_cpu_ids; i++)
		lp_dev_cpu[i].pwr_on.lvl = 0;
}

bool mtk_lp_plat_is_mcusys_off(void)
{
	return !lp_dev_mcusys.pwr_on.mcusys;
}

bool mtk_lp_plat_is_cluster_off(int cpu)
{
	if (cpu < 0)
		return false;

	if (unlikely(cpu >= nr_cpu_ids || !lp_dev_cpu[cpu].parent))
		return false;

	return !lp_dev_cpu[cpu].parent->pwr_on.cluster;
}

static int mtk_lp_cpuhp_notify_enter(unsigned int cpu)
{
	mtk_cpu_off_block();

	return 0;
}

static int mtk_lp_cpuhp_notify_leave(unsigned int cpu)
{
	mtk_lp_dev_set_cpus_off();
	mtk_lp_dev_get_cpus_online();
	mtk_cpu_off_allow();

	return 0;
}

static void mtk_lp_plat_cpuhp_init(void)
{
	cpuhp_setup_state_nocalls(CPUHP_BP_PREPARE_DYN_END, "cpuidle_cb",
				mtk_lp_cpuhp_notify_enter,
				mtk_lp_cpuhp_notify_leave);

	cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN, "cpuidle_cb",
				mtk_lp_cpuhp_notify_leave,
				mtk_lp_cpuhp_notify_enter);
}


#define DEPD_COND_TYPE_BOOTIME	(1<<0u)
#define DEPD_COND_TYPE_MCU	(1<<1u)

static int __mtk_lp_plat_wait_depd_condition(int type, void *arg)
{
	struct timespec uptime;
	bool mcupm_rdy = false;
	bool boot_time_pass = false;

	if (type & DEPD_COND_TYPE_MCU) {
		mtk_wait_mbox_init_done();
		mtk_notify_subsys_ap_ready();
	} else
		mcupm_rdy = true;

	do {
		msleep(1000);

		if (!mcupm_rdy && mtk_mcupm_is_ready())
			mcupm_rdy = true;

		if ((type & DEPD_COND_TYPE_BOOTIME) &&
		     !boot_time_pass) {
			get_monotonic_boottime(&uptime);

			if ((unsigned int)uptime.tv_sec > BOOT_TIME_LIMIT)
				boot_time_pass = true;
		}

	} while (!(mcupm_rdy && boot_time_pass));

	mtk_cpu_off_allow();
	mtk_lp_plat_cpuhp_init();

	return 0;
}

static int mtk_lp_plat_wait_depd_condition_nonmcu(void *arg)
{
	return __mtk_lp_plat_wait_depd_condition(DEPD_COND_TYPE_BOOTIME, arg);
}
static int mtk_lp_plat_wait_depd_condition(void *arg)
{
	return __mtk_lp_plat_wait_depd_condition((DEPD_COND_TYPE_BOOTIME
						| DEPD_COND_TYPE_MCU), arg);
}

static void __init mtk_lp_plat_pwr_dev_init(void)
{
	struct mtk_lp_device *dev;
	int i, cluster_id;

	for (i = 0; i < nr_cpu_ids; i++) {

		dev = &lp_dev_cpu[i];
		dev->pwr_lvl_mask = PWR_LVL_MASK(0);

		cluster_id = get_physical_cluster_id(i);

		if (cluster_id < nr_cluster_ids)
			dev->parent = &lp_dev_cluster[cluster_id];
	}

	for (i = 0; i < nr_cluster_ids; i++) {
		dev = &lp_dev_cluster[i];
		dev->pwr_lvl_mask = PWR_LVL_MASK(1);
		dev->parent = &lp_dev_mcusys;
	}

	lp_dev_mcusys.pwr_lvl_mask = PWR_LVL_MASK(2);

	mtk_lp_dev_get_cpus_online();
}

static int __init mtk_lp_plat_mcusys_ctrl_init(void)
{
	struct device_node *node = NULL;

	cpu_pm_syssram_base = NULL;

	node = of_find_compatible_node(NULL, NULL,
						"mediatek,cpupm-sysram");
	if (node) {
		cpu_pm_syssram_base = of_iomap(node, 0);
		of_node_put(node);
	}

	return plat_node_ready() ? 0 : -1;
}

int __init mtk_lp_plat_apmcu_init(void)
{
	struct device_node *node = NULL;
	unsigned int is_mcu_mode = 0;

	if (!plat_node_ready()) {
		mtk_lp_plat_qos_uninit();
		return 0;
	}

	mtk_lp_plat_pwr_dev_init();

	node = of_find_compatible_node(NULL, NULL, MTK_LPM_DTS_COMPATIBLE);
	if (node) {
		const char *method = NULL;

		of_property_read_string(node, "cpupm-method", &method);

		if (method && !strcmp(method, "mcu"))
			is_mcu_mode = 1;
		of_node_put(node);
	}

	if (is_mcu_mode)
		mtk_lp_plat_task =
			kthread_create(mtk_lp_plat_wait_depd_condition,
					NULL, "mtk_lp_plat_wait_rdy");
	else
		mtk_lp_plat_task =
			kthread_create(mtk_lp_plat_wait_depd_condition_nonmcu,
					NULL, "mtk_lp_plat_wait_rdy");

	if (!IS_ERR(mtk_lp_plat_task))
		wake_up_process(mtk_lp_plat_task);
	else
		pr_notice("Create thread fail @ %s()\n", __func__);

	return 0;
}

int __init mtk_lp_plat_apmcu_early_init(void)
{
	if (mtk_lp_plat_mcusys_ctrl_init() != 0) {
		pr_notice("%s(): Not support\n", __func__);
		return 0;
	}

	mtk_lp_plat_qos_init();

	if (cpu_pm_syssram_base) {
		memset_io((void __iomem *)
			(cpu_pm_syssram_base + LP_PM_SYSRAM_INFO_OFS),
			0,
			LP_PM_SYSRAM_SIZE - LP_PM_SYSRAM_INFO_OFS);
	}

	mtk_cpu_off_block();

	return 0;
}

