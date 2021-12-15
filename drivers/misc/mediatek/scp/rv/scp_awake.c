// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/io.h>
//#include <mt-plat/sync_write.h>
#include <mt-plat/aee.h>
#include <linux/delay.h>
#include "scp_feature_define.h"
#include "scp_ipi_pin.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_dvfs.h"
#include "scp.h"

#define SCP_SECURE_DUMP_MEASURE 0
#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM) && SCP_SECURE_DUMP_MEASURE
static struct cal {
	uint64_t start, end;
} scpdump_cal[3];
#endif

struct mutex scp_awake_mutexs[SCP_CORE_TOTAL];


/*
 * acquire scp lock flag, keep scp awake
 * @param scp_core_id: scp core id
 * return  0 :get lock success
 *        -1 :get lock timeout
 */
int scp_awake_lock(void *_scp_id)
{
	enum scp_core_id scp_id = (enum scp_core_id) _scp_id;
	unsigned long spin_flags;
	char *core_id;
	int *scp_awake_count;
	int count = 0;
	int ret = -1;
	unsigned int tmp;

	if (scp_id >= SCP_CORE_TOTAL) {
		pr_notice("%s: SCP ID >= SCP_CORE_TOTAL\n", __func__);
		return ret;
	}

	scp_awake_count = (int *)&scp_awake_counts[scp_id];
	core_id = core_ids[scp_id];

	if (is_scp_ready(scp_id) == 0) {
		pr_notice("%s: %s not enabled\n", __func__, core_id);
		return ret;
	}

	/* scp unlock awake */
	spin_lock_irqsave(&scp_awake_spinlock, spin_flags);
	if (*scp_awake_count > 0) {
		*scp_awake_count = *scp_awake_count + 1;
		spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);
		return 0;
	}

	/*set a direct IPI to awake SCP */
	/*pr_debug("scp_awake_lock: try to awake %s\n", core_id);*/
	writel(0xA0 | (1 << AP_AWAKE_LOCK), INFRA_IRQ_SET);

	count = 0;
	while (++count != SCP_AWAKE_TIMEOUT) {
#if SCP_RECOVERY_SUPPORT
		if (atomic_read(&scp_reset_status) == RESET_STATUS_START) {
			pr_notice("%s: resetting scp, break\n", __func__);
			break;
		}
#endif  // SCP_RECOVERY_SUPPORT

		tmp = readl(INFRA_IRQ_SET);
		if ((tmp & 0xA0) != 0xA0) {
			pr_notice("%s: INFRA_IRQ_SET %x\n", __func__, tmp);
			break;
		}
		if (!((tmp & 0x0f) & (1 << AP_AWAKE_LOCK))) {
			ret = 0;
			break;
		}
		udelay(10);
	}
	/* clear status */
	writel(0xA0 | (1 << AP_AWAKE_LOCK), INFRA_IRQ_CLEAR);

	/* scp lock awake success*/
	if (ret != -1)
		*scp_awake_count = *scp_awake_count + 1;

	if (ret == -1) {
		pr_notice("%s: awake %s fail..\n", __func__, core_id);
#if SCP_RECOVERY_SUPPORT
		if (atomic_read(&scp_reset_status) == RESET_STATUS_STOP) {
			scp_set_reset_status();
			pr_notice("%s: start to reset scp...\n", __func__);

#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
			if (scpreg.secure_dump) {
				scp_do_halt_set();
			} else {
#else
			{
#endif
			/* trigger halt isr, force scp enter wfi */
			writel(B_GIPC4_SETCLR_0, R_GIPC_IN_SET);
			}

			scp_send_reset_wq(RESET_TYPE_AWAKE);
		} else
			pr_notice("%s: scp resetting\n", __func__);
#endif
	}

	spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

	return ret;
}
EXPORT_SYMBOL_GPL(scp_awake_lock);

/*
 * release scp awake lock flag
 * @param scp_core_id: scp core id
 * return  0 :release lock success
 *        -1 :release lock fail
 */
int scp_awake_unlock(void *_scp_id)
{
	enum scp_core_id scp_id = (enum scp_core_id) _scp_id;
	unsigned long spin_flags;
	int *scp_awake_count;
	char *core_id;
	int count = 0;
	int ret = -1;
	unsigned int tmp;

	if (scp_id >= SCP_CORE_TOTAL) {
		pr_notice("%s: SCP ID >= SCP_CORE_TOTAL\n", __func__);
		return -1;
	}

	scp_awake_count = (int *)&scp_awake_counts[scp_id];
	core_id = core_ids[scp_id];

	if (is_scp_ready(scp_id) == 0) {
		pr_notice("%s: %s not enabled\n", __func__, core_id);
		return -1;
	}

	/* scp unlock awake */
	spin_lock_irqsave(&scp_awake_spinlock, spin_flags);
	if (*scp_awake_count > 1) {
		*scp_awake_count = *scp_awake_count - 1;
		spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);
		return 0;
	}

	/* WE1: set a direct IPI to release awake SCP */
	/*pr_debug("scp_awake_lock: try to awake %s\n", core_id);*/
	writel(0xA0 | (1 << AP_AWAKE_UNLOCK), INFRA_IRQ_SET);

	count = 0;
	while (++count != SCP_AWAKE_TIMEOUT) {
#if SCP_RECOVERY_SUPPORT
		if (atomic_read(&scp_reset_status) == RESET_STATUS_START) {
			pr_notice("%s: scp is being reset, break\n", __func__);
			break;
		}
#endif  // SCP_RECOVERY_SUPPORT
		tmp = readl(INFRA_IRQ_SET);
		if ((tmp & 0xA0) != 0xA0) {
			pr_notice("%s: INFRA7_IRQ_SET %x\n", __func__, tmp);
			break;
		}
		if (!((tmp & 0x0f) & (1 << AP_AWAKE_UNLOCK))) {
			ret = 0;
			break;
		}
		udelay(10);
	}
	/* clear status */
	writel(0xA0 | (1 << AP_AWAKE_UNLOCK), INFRA_IRQ_CLEAR);

	/* scp unlock awake success*/
	if (ret != -1) {
		if (*scp_awake_count <= 0)
			pr_err("%s:%s awake_count=%d NOT SYNC!\n", __func__,
			core_id, *scp_awake_count);

		if (*scp_awake_count > 0)
			*scp_awake_count = *scp_awake_count - 1;
	}

	if (ret == -1) {
		pr_notice("%s: awake %s fail..\n", __func__, core_id);
#if SCP_RECOVERY_SUPPORT
		if (atomic_read(&scp_reset_status) == RESET_STATUS_STOP) {
			scp_set_reset_status();
			pr_notice("%s: start to reset scp...\n", __func__);

#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
			if (scpreg.secure_dump) {
				scp_do_halt_set();
			} else {
#else
			{
#endif
			/* trigger halt isr, force scp enter wfi */
			writel(B_GIPC4_SETCLR_0, R_GIPC_IN_SET);
			}

			scp_send_reset_wq(RESET_TYPE_AWAKE);
		} else
			pr_notice("%s: scp resetting\n", __func__);
#endif
	}


	/* spinlock context safe */
	spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

	return ret;
}
EXPORT_SYMBOL_GPL(scp_awake_unlock);

void scp_enable_sram(void)
{
	uint32_t reg_temp;

	/*enable sram, enable 1 block per time*/
	for (reg_temp = 0xffffffff; reg_temp != 0;) {
		reg_temp = reg_temp >> 1;
		writel(reg_temp, SCP_CPU0_SRAM_PD);
		writel(reg_temp, SCP_CPU1_SRAM_PD);
	}
	/*enable scp all TCM*/
	writel(0, SCP_CLK_CTRL_L1_SRAM_PD);
	writel(0, SCP_CLK_CTRL_TCM_TAIL_SRAM_PD);
}

/*
 * scp_sys_reset, reset scp
 */
int scp_sys_full_reset(void)
{
	void *tmp;
#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM) && SCP_SECURE_DUMP_MEASURE
	int idx;
#endif
#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
	uint64_t restore_start, restore_end;
#endif

	pr_notice("[SCP] %s\n", __func__);

#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
	restore_start = ktime_get_boot_ns();
	if (scpreg.secure_dump) {
#if SCP_SECURE_DUMP_MEASURE
		memset(scpdump_cal, 0x0, sizeof(scpdump_cal));
		scpdump_cal[0].start = ktime_get_boot_ns();
		scpdump_cal[1].start = ktime_get_boot_ns();
#endif

		scp_restore_l2tcm();

#if SCP_SECURE_DUMP_MEASURE
		scpdump_cal[1].end = ktime_get_boot_ns();
#endif
	} else {
#else
	{
#endif
	/* clear whole TCM */
	memset_io(SCP_TCM, 0, SCP_TCM_SIZE);
	/*copy loader to scp sram*/
	memcpy_to_scp(SCP_TCM, (const void *)(size_t)scp_loader_virt
		, scp_region_info_copy.ap_loader_size);
	/*set info to sram*/
	memcpy_to_scp(scp_region_info, (const void *)&scp_region_info_copy
			, sizeof(scp_region_info_copy));
	}

#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
	if (scpreg.secure_dump) {
#if SCP_SECURE_DUMP_MEASURE
		scpdump_cal[2].start = ktime_get_boot_ns();
#endif
		tmp = (void *)(scp_ap_dram_virt +
			ROUNDUP(scp_region_info_copy.ap_dram_size, 1024)
			* scpreg.core_nums);
		scp_restore_dram();
#if SCP_SECURE_DUMP_MEASURE
		scpdump_cal[2].end = ktime_get_boot_ns();
		scpdump_cal[0].end = ktime_get_boot_ns();
		for (idx = 0; idx < 3; idx++) {
			pr_notice("MDebug SCP Cal:%d %lldns\n", idx,
					(scpdump_cal[idx].end - scpdump_cal[idx].start));
		}
#endif
		restore_end = ktime_get_boot_ns();
		pr_notice("[SCP] Restore: %lld ns\n", (restore_end - restore_start));
	} else {
#else
	{
#endif
	/* reset dram from dram back */
	if ((int)(scp_region_info_copy.ap_dram_size) > 0) {
		tmp = (void *)(scp_ap_dram_virt +
			ROUNDUP(scp_region_info_copy.ap_dram_size, 1024)
			* scpreg.core_nums);
		memset(scp_ap_dram_virt, 0,
			ROUNDUP(scp_region_info_copy.ap_dram_size, 1024)
			* scpreg.core_nums);
		memcpy(scp_ap_dram_virt, tmp,
			scp_region_info_copy.ap_dram_size);
	}
	}
	return 0;
}

int scp_clr_spm_reg(void *unused)
{
	/* AP side write 0x1 to SCP2SPM_IPC_CLR to clear
	 * scp side write 0x1 to SCP2SPM_IPC_SET to set SPM reg
	 * scp set        bit[0]
	 */
	writel(0x1, SCP_TO_SPM_REG);

	return 0;
}
EXPORT_SYMBOL_GPL(scp_clr_spm_reg);

