/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>

#include <mt-plat/mtk_meminfo.h> /* dcs_get_dcs_status_trylock/unlock */
#include <mt-plat/mtk_boot.h>

#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
#include <teei_client_main.h> /* is_teei_ready */
#endif

#include <mtk_spm_resource_req.h>
#include <mtk_idle.h>
#include <mtk_idle_internal.h>
#include <mtk_spm_internal.h> /* mtk_idle_cond_update_state */


/* [ByChip] Internal weak functions: implemented in mtk_spm.c */
int __attribute__((weak)) spm_load_firmware_status(void) { return -1; }
/* [ByChip] Internal weak functions: implemented in mtk_idle_cond_check.c */
void __attribute__((weak)) mtk_idle_cond_update_state(void) {}

/* [ByChip] Internal weak functions:
 * If platform need to blocked idle task by specific define.
 * Please implement it in platform folder
 */
int __attribute__((weak)) mtk_idle_plat_bootblock_check(void)
{
	return MTK_IDLE_PLAT_READY;
}

#if defined(MTK_IDLE_DVT_TEST_ONLY)

#include <linux/cpu.h>
static atomic_t is_in_hotplug = ATOMIC_INIT(0);

static bool mtk_idle_cpu_criteria(void)
{
	return ((atomic_read(&is_in_hotplug) == 1) ||
		(num_online_cpus() != 1)) ? false : true;
}

static int mtk_idle_cpu_callback(struct notifier_block *nfb,
	unsigned long action, void *hcpu)
{
	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		atomic_inc(&is_in_hotplug);
		break;

	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
	case CPU_DOWN_FAILED:
	case CPU_DOWN_FAILED_FROZEN:
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		atomic_dec(&is_in_hotplug);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block mtk_idle_cpu_notifier = {
	.notifier_call = mtk_idle_cpu_callback,
	.priority   = INT_MAX,
};

int __init mtk_idle_hotplug_cb_init(void)
{
	register_cpu_notifier(&mtk_idle_cpu_notifier);

	return 0;
}

late_initcall(mtk_idle_hotplug_cb_init);

static void __go_to_wfi(int cpu)
{
	isb();
	/* memory barrier before WFI */
	mb();
	__asm__ __volatile__("wfi" : : : "memory");
}

int mtk_idle_enter_dvt(int cpu)
{
	int state = -1;

	if (mtk_idle_cpu_criteria())
		state = mtk_idle_select(cpu);

	switch (state) {
	case IDLE_TYPE_DP:
		dpidle_enter(cpu);
		break;
	case IDLE_TYPE_SO3:
		soidle3_enter(cpu);
		break;
	case IDLE_TYPE_SO:
		soidle_enter(cpu);
		break;
	default:
		__go_to_wfi(cpu);
		break;
	}

	return 0;
}
#endif /* MTK_IDLE_DVT_TEST_ONLY */

#if defined(CONFIG_MTK_UFS_SUPPORT)
static unsigned int idle_ufs_lock;
static DEFINE_SPINLOCK(idle_ufs_spin_lock);

void idle_lock_by_ufs(unsigned int lock)
{
	unsigned long flags;

	spin_lock_irqsave(&idle_ufs_spin_lock, flags);
	idle_ufs_lock = lock;
	spin_unlock_irqrestore(&idle_ufs_spin_lock, flags);
}
#endif

static int check_each_idle_type(int reason)
{

	if (mtk_idle_screen_off_sodi3) {
		if (sodi3_can_enter(reason))
			return IDLE_TYPE_SO3;
		else if (dpidle_can_enter(reason))
			return IDLE_TYPE_DP;
		else if (sodi_can_enter(reason))
			return IDLE_TYPE_SO;
	} else {
		if (dpidle_can_enter(reason))
			return IDLE_TYPE_DP;
		else if (sodi3_can_enter(reason))
			return IDLE_TYPE_SO3;
		else if (sodi_can_enter(reason))
			return IDLE_TYPE_SO;
	}

	/* always can enter rgidle */
	return IDLE_TYPE_RG;
}

int mtk_idle_select(int cpu)
{
	int idx;
	int reason = NR_REASONS;
	#if defined(CONFIG_MTK_UFS_SUPPORT)
	unsigned long flags = 0;
	unsigned int ufs_locked;
	int boot_type;
	#endif
	#if defined(CONFIG_MTK_DCS)
	int ch = 0, ret = -1;
	enum dcs_status dcs_status;
	bool dcs_lock_get = false;
	#endif

	/* direct return if all mtk idle features are off */
	if (!mtk_dpidle_enabled() &&
		!mtk_sodi3_enabled() && !mtk_sodi_enabled()) {
		return -1;
	}

	/* If kernel didn't enter system running state,
	 *  idle task can't enter mtk idle.
	 */
	if (!((system_state == SYSTEM_RUNNING) &&
		(mtk_idle_plat_bootblock_check() == MTK_IDLE_PLAT_READY))
	) {
		pr_notice("Power/swap %s blocked by boot time\n", __func__);
		return -1;
	}

	__profile_idle_start(IDLE_TYPE_DP, PIDX_SELECT_TO_ENTER);
	__profile_idle_start(IDLE_TYPE_SO3, PIDX_SELECT_TO_ENTER);
	__profile_idle_start(IDLE_TYPE_SO, PIDX_SELECT_TO_ENTER);

	/* 1. spmfw firmware is loaded ? */
	#if !defined(CONFIG_FPGA_EARLY_PORTING)
	/* return -1: not init, 0: not loaded, */
	/* 1: loaded, 2: loaded and kicked */
	if (spm_load_firmware_status() < 2) {
		reason = BY_FRM;
		goto get_idle_idx;
	}
	#endif

	/* 2. spm resources are all used currently ? */
	if (spm_get_resource_usage() == SPM_RESOURCE_ALL) {
		reason = BY_SRR;
		goto get_idle_idx;
	}

	/* 3. locked by ufs ? */
	#if defined(CONFIG_MTK_UFS_SUPPORT)
	boot_type = get_boot_type();
	if (boot_type == BOOTDEV_UFS) {
		spin_lock_irqsave(&idle_ufs_spin_lock, flags);
		ufs_locked = idle_ufs_lock;
		spin_unlock_irqrestore(&idle_ufs_spin_lock, flags);

		if (ufs_locked) {
			reason = BY_UFS;
			goto get_idle_idx;
		}
	}
	#endif

	/* 4. tee is ready ? */
	#if !defined(CONFIG_FPGA_EARLY_PORTING) && \
		defined(CONFIG_MICROTRUST_TEE_SUPPORT)
	if (!is_teei_ready()) {
		reason = BY_TEE;
		goto get_idle_idx;
	}
	#endif

	/* Update current idle condition state for later check */
	mtk_idle_cond_update_state();

	/* 5. is dcs channel switching ? */
	#if defined(CONFIG_MTK_DCS)
	/* check if DCS channel switching */
	ret = dcs_get_dcs_status_trylock(&ch, &dcs_status);
	if (ret) {
		reason = BY_DCS;
		goto get_idle_idx;
	}

	dcs_lock_get = true;
	#endif

get_idle_idx:
	idx = check_each_idle_type(reason);

	#if defined(CONFIG_MTK_DCS)
	if (dcs_lock_get)
		dcs_get_dcs_status_unlock();
	#endif

	return idx;
}

