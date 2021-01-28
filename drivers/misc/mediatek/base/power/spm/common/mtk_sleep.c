// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/console.h>

#include <mtk_sleep_internal.h>
#include <mtk_spm_internal.h> /* mtk_idle_cond_check */
#include <mtk_spm_suspend_internal.h>
#include <mtk_idle_sysfs.h>
#include <mtk_power_gs_api.h>
#include <mtk_idle.h>
#include <mtk_idle_internal.h>
#ifdef CONFIG_MTK_SND_SOC_NEW_ARCH
#include <mtk-soc-afe-control.h>
#endif /* CONFIG_MTK_SND_SOC_NEW_ARCH */

#ifdef CONFIG_SND_SOC_MTK_SMART_PHONE
#include <mtk-sp-afe-external.h>
#define ConditionEnterSuspend mtk_audio_condition_enter_suspend
#endif /* CONFIG_SND_SOC_MTK_SMART_PHONE */

#include <mtk_mcdi_api.h>

#include <mtk_lp_dts.h>
static DEFINE_SPINLOCK(slp_lock);

unsigned long slp_dp_cnt[NR_CPUS] = {0};
static unsigned int slp_wake_reason = WR_NONE;

static bool slp_suspend_ops_valid_on;
static bool slp_ck26m_on;

bool slp_dump_gpio;
bool slp_dump_golden_setting;
int slp_dump_golden_setting_type = GS_PMIC;

static int slp_suspend_ops_valid(suspend_state_t state)
{
	if (slp_suspend_ops_valid_on)
		return state == PM_SUSPEND_MEM;
	else
		return 0;
}

static int slp_suspend_ops_begin(suspend_state_t state)
{
	/* legacy log */
	pr_info("[SLP] @@@@@@@@@@@@@@@@\tChip_pm_begin(%u)(%u)\t@@@@@@@@@@@@@@@@\n",
			spm_get_is_cpu_pdn(), spm_get_is_infra_pdn());

	slp_wake_reason = WR_NONE;

	return 0;
}

static int slp_suspend_ops_prepare(void)
{
	return 0;
}

#if defined(CONFIG_MTK_SND_SOC_NEW_ARCH) \
|| defined(CONFIG_SND_SOC_MTK_SMART_PHONE)
bool __attribute__ ((weak)) ConditionEnterSuspend(void)
{
	pr_info("NO %s !!!\n", __func__);
	return true;
}
#endif /* MTK_SUSPEND_AUDIO_SUPPORT */

#ifdef CONFIG_MTK_SYSTRACKER
void __attribute__ ((weak)) systracker_enable(void)
{
	pr_info("NO %s !!!\n", __func__);
}
#endif /* CONFIG_MTK_SYSTRACKER */

#ifdef CONFIG_MTK_BUS_TRACER
void __attribute__ ((weak)) bus_tracer_enable(void)
{
	pr_info("NO %s !!!\n", __func__);
}
#endif /* CONFIG_MTK_BUS_TRACER */

void __attribute__((weak)) subsys_if_on(void)
{
	pr_info("NO %s !!!\n", __func__);
}

void __attribute__((weak)) pll_if_on(void)
{
	pr_info("NO %s !!!\n", __func__);
}

void __attribute__((weak))
gpio_dump_regs(void)
{

}

void __attribute__((weak))
spm_output_sleep_option(void)
{

}

int __attribute__((weak))
spm_set_sleep_wakesrc(u32 wakesrc, bool enable, bool replace)
{
	return 0;
}

bool __attribute__((weak)) spm_is_enable_sleep(void)
{
	pr_info("NO %s !!!\n", __func__);
	return false;
}

void __attribute__((weak))
mtk_suspend_cond_info(void)
{
}

unsigned int __attribute__((weak))
spm_go_to_sleep(void)
{
	return 0;
}

bool __attribute__((weak))
spm_get_is_cpu_pdn(void)
{
	pr_info("NO %s !!!\n", __func__);
	return false;
}

bool __attribute__((weak))
spm_get_is_infra_pdn(void)
{
	pr_info("NO %s !!!\n", __func__);
	return false;
}

static int slp_suspend_ops_enter(suspend_state_t state)
{
	int ret = 0;

#if SLP_SLEEP_DPIDLE_EN
#if defined(CONFIG_MTK_SND_SOC_NEW_ARCH) \
|| defined(CONFIG_SND_SOC_MTK_SMART_PHONE)
	int fm_radio_is_playing = 0;

	if (ConditionEnterSuspend() == true)
		fm_radio_is_playing = 0;
	else
		fm_radio_is_playing = 1;
#endif /* CONFIG_MTK_SND_SOC_NEW_ARCH || CONFIG_SND_SOC_MTK_SMART_PHONE */
#endif

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (slp_dump_gpio)
		gpio_dump_regs();
#endif /* CONFIG_FPGA_EARLY_PORTING */

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	pll_if_on();
	subsys_if_on();
#endif /* CONFIG_FPGA_EARLY_PORTING */

	if (spm_get_is_infra_pdn() && !spm_get_is_cpu_pdn()) {
		pr_info("[SLP] CANNOT SLEEP DUE TO INFRA PDN BUT CPU PDN\n");
		ret = -EPERM;
		goto LEAVE_SLEEP;
	}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	if (is_sspm_ipi_lock_spm()) {
		pr_info("[SLP] CANNOT SLEEP DUE TO SSPM IPI\n");
		ret = -EPERM;
		goto LEAVE_SLEEP;
	}
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (spm_load_firmware_status() < 1) {
		pr_info("SPM FIRMWARE IS NOT READY\n");
		ret = -EPERM;
		goto LEAVE_SLEEP;
	}
#endif /* CONFIG_FPGA_EARLY_PORTING */

	mcdi_task_pause(true);

	mtk_idle_cond_update_state();

#if SLP_SLEEP_DPIDLE_EN
#if defined(CONFIG_MTK_SND_SOC_NEW_ARCH) \
|| defined(CONFIG_SND_SOC_MTK_SMART_PHONE)
	if (slp_ck26m_on | fm_radio_is_playing) {
#else
	if (slp_ck26m_on) {
#endif /* CONFIG_MTK_SND_SOC_NEW_ARCH */
		mtk_idle_enter(IDLE_TYPE_DP, smp_processor_id(),
					MTK_IDLE_OPT_SLEEP_DPIDLE, 0);
		slp_wake_reason = get_slp_dp_last_wr();
		slp_dp_cnt[smp_processor_id()]++;
	} else {
#endif
		mtk_suspend_cond_info();

		slp_wake_reason = spm_go_to_sleep();
	}

	mcdi_task_pause(false);

LEAVE_SLEEP:
#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifdef CONFIG_MTK_SYSTRACKER
	systracker_enable();
#endif
#ifdef CONFIG_MTK_BUS_TRACER
	bus_tracer_enable();
#endif
#endif /* CONFIG_FPGA_EARLY_PORTING */

	return ret;
}

static void slp_suspend_ops_finish(void)
{
}

static void slp_suspend_ops_end(void)
{
}

static const struct platform_suspend_ops slp_suspend_ops = {
	.valid = slp_suspend_ops_valid,
	.begin = slp_suspend_ops_begin,
	.prepare = slp_suspend_ops_prepare,
	.enter = slp_suspend_ops_enter,
	.finish = slp_suspend_ops_finish,
	.end = slp_suspend_ops_end,
};

__attribute__ ((weak))
int spm_set_dpidle_wakesrc(u32 wakesrc, bool enable, bool replace)
{
	pr_info("NO %s !!!\n", __func__);
	return 0;
}

/*
 * wakesrc : WAKE_SRC_XXX
 * enable  : enable or disable @wakesrc
 * ck26m_on: if true, mean @wakesrc needs 26M to work
 */
int slp_set_wakesrc(u32 wakesrc, bool enable, bool ck26m_on)
{
	int r;
	unsigned long flags;

	pr_info("[SLP] wakesrc = 0x%x, enable = %u, ck26m_on = %u\n",
		wakesrc, enable, ck26m_on);

#if SLP_REPLACE_DEF_WAKESRC
	if (wakesrc & WAKE_SRC_CFG_KEY)
#else
	if (!(wakesrc & WAKE_SRC_CFG_KEY))
#endif
		return -EPERM;

	spin_lock_irqsave(&slp_lock, flags);

#if SLP_REPLACE_DEF_WAKESRC
	if (ck26m_on)
		r = spm_set_dpidle_wakesrc(wakesrc, enable, true);
	else
		r = spm_set_sleep_wakesrc(wakesrc, enable, true);
#else
	if (ck26m_on)
		r = spm_set_dpidle_wakesrc(wakesrc & ~WAKE_SRC_CFG_KEY,
				enable, false);
	else
		r = spm_set_sleep_wakesrc(wakesrc & ~WAKE_SRC_CFG_KEY, enable,
				false);
#endif

	if (!r)
		slp_ck26m_on = ck26m_on;
	spin_unlock_irqrestore(&slp_lock, flags);
	return r;
}

unsigned int slp_get_wake_reason(void)
{
	return slp_wake_reason;
}
EXPORT_SYMBOL(slp_get_wake_reason);

/*
 * debugfs
 */
static ssize_t suspend_state_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	int i;

	if (!ToUserBuf)
		return -EINVAL;
	#undef log
	#define log(fmt, args...) ({\
		p += scnprintf(p, sz - strlen(ToUserBuf), fmt, ##args); p; })

	log("*********** suspend state ************\n");
	log("suspend valid status = %d\n",
		       slp_suspend_ops_valid_on);
	log("*************** slp dp cnt ***********\n");
	for (i = 0; i < nr_cpu_ids; i++)
		log("cpu%d: slp_dp=%lu\n", i, slp_dp_cnt[i]);

	log("*********** suspend command ************\n");
	log("echo suspend 1/0 > /sys/kernel/debug/cpuidle/slp/suspend_state\n");

	return p - ToUserBuf;
}

static ssize_t suspend_state_write(char *FromUserBuf, size_t sz, void *priv)
{
	char cmd[128];
	int param;

	if (!FromUserBuf)
		return -EINVAL;

	if (sscanf(FromUserBuf, "%127s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "suspend")) {
			/* update suspend valid status */
			slp_suspend_ops_valid_on = param;

			/* suspend reinit ops */
			suspend_set_ops(&slp_suspend_ops);
		}
		return sz;
	}

	return -EINVAL;
}

static const struct mtk_idle_sysfs_op suspend_state_fops = {
	.fs_read = suspend_state_read,
	.fs_write = suspend_state_write,
};

void slp_module_init(void)
{
	struct mtk_idle_sysfs_handle pParent2ND;
	struct mtk_idle_sysfs_handle *pParent = NULL;
	struct device_node *idle_node = NULL;

	/* Get dts of cpu's idle-state*/
	idle_node = GET_MTK_IDLE_STATES_DTS_NODE();

	if (idle_node) {
		int state = 0;

		/* Get dts of SUSPEDN*/
		state = GET_MTK_OF_PROPERTY_STATUS_SUSPEND(idle_node);
		if (state & MTK_OF_PROPERTY_STATUS_FOUND) {
			if (state & MTK_OF_PROPERTY_VALUE_ENABLE)
				slp_suspend_ops_valid_on = true;
			else
				slp_suspend_ops_valid_on = false;
		} else
			slp_suspend_ops_valid_on = spm_is_enable_sleep();

		of_node_put(idle_node);
	}

	mtk_idle_sysfs_entry_create();
	if (mtk_idle_sysfs_entry_root_get(&pParent) == 0) {
		mtk_idle_sysfs_entry_func_create("slp", 0444
			, pParent, &pParent2ND);
		mtk_idle_sysfs_entry_func_node_add("suspend_state", 0444
			, &suspend_state_fops, &pParent2ND, NULL);
	}

	spm_output_sleep_option();
	pr_info("[SLP] SLEEP_DPIDLE_EN:%d, REPLACE_DEF_WAKESRC:%d",
		SLP_SLEEP_DPIDLE_EN, SLP_REPLACE_DEF_WAKESRC);
	pr_info(", SUSPEND_LOG_EN:%d\n", SLP_SUSPEND_LOG_EN);
	suspend_set_ops(&slp_suspend_ops);
#if SLP_SUSPEND_LOG_EN
	console_suspend_enabled = 0;
#endif
}

module_param(slp_ck26m_on, bool, 0644);

module_param(slp_dump_gpio, bool, 0644);
module_param(slp_dump_golden_setting, bool, 0644);
module_param(slp_dump_golden_setting_type, int, 0644);

MODULE_DESCRIPTION("Sleep Driver v0.1");
