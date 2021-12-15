/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

#include <mtk_idle_module.h>

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

static bool mtk_idle_cpu_criteria(void)
{
	return (num_online_cpus() != 1) ? false : true;
}

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

	if (mtk_idle_cpu_criteria()) {
		if (mtk_idle_select(cpu) != IDLE_TYPE_LEGACY_ENTER)
			__go_to_wfi(cpu);
		else
			state = 0;
	}

	return state;
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

#define LOG_STR "blocked by boot reason, system_state = "
/* mtk_idle_select */
int mtk_idle_entrance(struct mtk_idle_info *info
	, int *ChosenIdle, int IsSelectOnly)
{
	int idx = 0;
	int reason = NR_REASONS;
	int bRet = 0;
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
	if (mtk_idle_module_enabled() != MTK_IDLE_MOD_OK)
		return -1;

	/* If kernel didn't enter system running state,
	 *  idle task can't enter mtk idle.
	 */
	if (!((system_state == SYSTEM_RUNNING) &&
		(mtk_idle_plat_bootblock_check() == MTK_IDLE_PLAT_READY))
	) {
		if (system_state < SYSTEM_RUNNING) {
			printk_deferred("[name:spm&]Power/swap %s %s %d\n"
					, __func__, LOG_STR, system_state);
		}

		return -1;
	}

	__profile_idle_start(PIDX_SELECT_TO_ENTER);

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

	#if defined(CONFIG_MTK_DCS)
	if (dcs_lock_get)
		dcs_get_dcs_status_unlock();
	#endif

	if (IsSelectOnly != 1) {
		if (!MTK_IDLE_MOD_SUCCESS(
			mtk_idle_module_enter(info, reason, &idx))
		)
			bRet = -1;
	} else {
		if (!MTK_IDLE_MOD_SUCCESS(
			mtk_idle_module_model_sel(info, reason, &idx))
		)
			bRet = -1;
	}

	if (ChosenIdle)
		*ChosenIdle = idx;

	return bRet;
}

/* Stub function, maybe remove later */
int mtk_idle_select(int cpu)
{
	int idleIdx = 0;
	struct mtk_idle_info IdleInfo = {
		.cpu = cpu,
		.predit_us = 0xffffffff,
	};

	if (mtk_idle_entrance(&IdleInfo, &idleIdx, 0) == 0)
		idleIdx = IDLE_TYPE_LEGACY_ENTER;
	else
		idleIdx = IDLE_TYPE_LEGACY_CAN_NOT_ENTER;

	return idleIdx;
}
