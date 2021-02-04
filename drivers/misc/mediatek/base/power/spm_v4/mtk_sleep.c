/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/console.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>

#include <mt-plat/sync_write.h>
#include <mtk_sleep.h>
#include <mtk_spm.h>
#include <mtk_spm_sleep.h>
#include <mtk_spm_idle.h>
#include <mtk_spm_misc.h>
#include <mtk_power_gs_api.h>
#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
#include <mt-plat/upmu_common.h>
#include <include/pmic.h>
#endif

#ifdef CONFIG_MTK_SND_SOC_NEW_ARCH
#include <mtk-soc-afe-control.h>
#endif /* CONFIG_MTK_SND_SOC_NEW_ARCH */

#if 1 /*removed : #ifdef CONFIG_MTK_ACAO_SUPPORT*/
#include <mtk_mcdi_api.h>
#endif
/**************************************
 * only for internal debug
 **************************************/
#ifdef CONFIG_MTK_LDVT
#define SLP_SLEEP_DPIDLE_EN         1
#define SLP_REPLACE_DEF_WAKESRC     1
#define SLP_SUSPEND_LOG_EN          1
#else
#define SLP_SLEEP_DPIDLE_EN         1
#define SLP_REPLACE_DEF_WAKESRC     0
#define SLP_SUSPEND_LOG_EN          1
#endif

/**************************************
 * SW code for suspend
 **************************************/
#define slp_read(addr)              __raw_readl((void __force __iomem *)(addr))
#define slp_write(addr, val)        mt65xx_reg_sync_writel(val, addr)
#define slp_emerg(fmt, args...)     pr_debug("[SLP] " fmt, ##args)
#define slp_alert(fmt, args...)     pr_debug("[SLP] " fmt, ##args)
#define slp_crit(fmt, args...)      pr_debug("[SLP] " fmt, ##args)
#define slp_crit2(fmt, args...)     pr_debug("[SLP] " fmt, ##args)
#define slp_warning(fmt, args...)   pr_debug("[SLP] " fmt, ##args)
#define slp_notice(fmt, args...)    pr_debug("[SLP] " fmt, ##args)
#define slp_info(fmt, args...)      pr_debug("[SLP] " fmt, ##args)
#define slp_debug(fmt, args...)     pr_debug("[SLP] " fmt, ##args)
static DEFINE_SPINLOCK(slp_lock);

static unsigned int slp_wake_reason = WR_NONE;

static bool slp_suspend_ops_valid_on;
static bool slp_ck26m_on;
bool slp_dump_gpio;
bool slp_dump_golden_setting;
int slp_dump_golden_setting_type = GS_PMIC;

#if defined(CONFIG_MACH_MT6763)
/* FIXME: */
static u32 slp_spm_flags = {
	/* SPM_FLAG_DIS_CPU_PDN | */
	/* SPM_FLAG_DIS_INFRA_PDN | */
	/* SPM_FLAG_DIS_DDRPHY_PDN | */
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH |
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_SUSPEND_OPTION
};
#if SLP_SLEEP_DPIDLE_EN
/* sync with mt_idle.c spm_deepidle_flags setting */
/* FIXME: */
static u32 slp_spm_deepidle_flags = {
	/* SPM_FLAG_DIS_CPU_PDN | */
	/* SPM_FLAG_DIS_INFRA_PDN | */
	/* SPM_FLAG_DIS_DDRPHY_PDN | */
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH |
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_DEEPIDLE_OPTION
};
#endif
#elif defined(CONFIG_MACH_MT6739)
#if 1
/* FIXME: */
static u32 slp_spm_flags = {
	/* SPM_FLAG_DIS_CPU_PDN | */
	/* SPM_FLAG_DIS_INFRA_PDN | */
	/* SPM_FLAG_DIS_DDRPHY_PDN | */
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	/* SPM_FLAG_DIS_VPROC_VSRAM_DVS | */
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH
};
#if SLP_SLEEP_DPIDLE_EN
/* sync with mt_idle.c spm_deepidle_flags setting */
/* FIXME: */
static u32 slp_spm_deepidle_flags = {
	/* SPM_FLAG_DIS_CPU_PDN | */
	/* SPM_FLAG_DIS_INFRA_PDN | */
	/* SPM_FLAG_DIS_DDRPHY_PDN | */
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	/* SPM_FLAG_DIS_VPROC_VSRAM_DVS | */
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH
};
#endif
#else
/* FIXME: */
static u32 slp_spm_flags = {
	/* SPM_FLAG_DIS_CPU_PDN | */
	SPM_FLAG_DIS_INFRA_PDN |
	SPM_FLAG_DIS_DDRPHY_PDN |
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_DIS_VPROC_VSRAM_DVS |
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH
};
#if SLP_SLEEP_DPIDLE_EN
/* sync with mt_idle.c spm_deepidle_flags setting */
/* FIXME: */
static u32 slp_spm_deepidle_flags = {
	/* SPM_FLAG_DIS_CPU_PDN | */
	SPM_FLAG_DIS_INFRA_PDN |
	SPM_FLAG_DIS_DDRPHY_PDN |
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_DIS_VPROC_VSRAM_DVS |
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH
};
#endif
#endif
#elif defined(CONFIG_MACH_MT6771)
/* FIXME: */
static u32 slp_spm_flags = {
	/* SPM_FLAG_DIS_CPU_PDN | */
	/* SPM_FLAG_DIS_INFRA_PDN | */
	/* SPM_FLAG_DIS_DDRPHY_PDN | */
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	/* SPM_FLAG_DIS_VPROC_VSRAM_DVS | */
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_SUSPEND_OPTION
};

static u32 slp_spm_deepidle_flags = {
	/* SPM_FLAG_DIS_CPU_PDN | */
	/* SPM_FLAG_DIS_INFRA_PDN | */
	/* SPM_FLAG_DIS_DDRPHY_PDN | */
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	/* SPM_FLAG_DIS_VPROC_VSRAM_DVS | */
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_DEEPIDLE_OPTION
};
#endif /* CONFIG_MACH_MT6771 */

#if defined(CONFIG_MACH_MT6771)

static u32 slp_spm_flags1 = {
	0
};

static u32 slp_spm_deepidle_flags1 = {
	0
};

#else
static u32 slp_spm_flags1;
static u32 slp_spm_deepidle_flags1;
#endif

static int slp_suspend_ops_valid(suspend_state_t state)
{
	if (slp_suspend_ops_valid_on)
		return state == PM_SUSPEND_MEM;
	else
		return false;
}

static int slp_suspend_ops_begin(suspend_state_t state)
{
	/* legacy log */
	slp_notice("@@@@@@@@@@@@@@@@@@@@\tChip_pm_begin(%u)(%u)\t@@@@@@@@@@@@@@@@@@@@\n",
			is_cpu_pdn(slp_spm_flags), is_infra_pdn(slp_spm_flags));

	slp_wake_reason = WR_NONE;

	return 0;
}

static int slp_suspend_ops_prepare(void)
{
	/* legacy log */
	/* slp_crit2("@@@@@@\tChip_pm_prepare\t@@@@@@\n"); */

	return 0;
}

#ifdef CONFIG_MTK_SND_SOC_NEW_ARCH
bool __attribute__ ((weak)) ConditionEnterSuspend(void)
{
	pr_err("NO %s !!!\n", __func__);
	return true;
}
#endif /* MTK_SUSPEND_AUDIO_SUPPORT */

#ifdef CONFIG_MTK_SYSTRACKER
void __attribute__ ((weak)) systracker_enable(void)
{
	pr_err("NO %s !!!\n", __func__);
}
#endif /* CONFIG_MTK_SYSTRACKER */

#ifdef CONFIG_MTK_BUS_TRACER
void __attribute__ ((weak)) bus_tracer_enable(void)
{
	pr_err("NO %s !!!\n", __func__);
}
#endif /* CONFIG_MTK_BUS_TRACER */

__attribute__ ((weak))
unsigned int spm_go_to_sleep_dpidle(u32 spm_flags, u32 spm_data)
{
	pr_err("NO %s !!!\n", __func__);
	return WR_NONE;
}

void __attribute__((weak)) subsys_if_on(void)
{
	pr_err("NO %s !!!\n", __func__);
}

void __attribute__((weak)) pll_if_on(void)
{
	pr_err("NO %s !!!\n", __func__);
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

unsigned int __attribute__((weak))
spm_go_to_sleep(u32 spm_flags, u32 spm_data)
{
	return 0;
}

void __attribute__((weak))
gpio_dump_regs(void)
{

}

static int slp_suspend_ops_enter(suspend_state_t state)
{
	int ret = 0;
#if defined(CONFIG_MACH_MT6739)
#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
	unsigned int pmic_ver = PMIC_LP_CHIP_VER();
#endif
#endif

#if SLP_SLEEP_DPIDLE_EN
#ifdef CONFIG_MTK_SND_SOC_NEW_ARCH
	int fm_radio_is_playing = 0;

	if (ConditionEnterSuspend() == true)
		fm_radio_is_playing = 0;
	else
		fm_radio_is_playing = 1;
#endif /* CONFIG_MTK_SND_SOC_NEW_ARCH */
#endif


	/* legacy log */
	/* slp_crit2("@@@@@@\tChip_pm_enter\t@@@@@@\n"); */

#if defined(CONFIG_MACH_MT6739)
#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
	if (pmic_ver == 1) {
		pr_err("set SPM_FLAG_DIS_VPROC_VSRAM_DVS for pmic issue\n");
		slp_spm_flags |= SPM_FLAG_DIS_VPROC_VSRAM_DVS;
		slp_spm_deepidle_flags |= SPM_FLAG_DIS_VPROC_VSRAM_DVS;
	}
#endif
#endif

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (slp_dump_gpio)
		gpio_dump_regs();
#endif /* CONFIG_FPGA_EARLY_PORTING */

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	pll_if_on();
	subsys_if_on();
#endif /* CONFIG_FPGA_EARLY_PORTING */

	if (is_infra_pdn(slp_spm_flags) && !is_cpu_pdn(slp_spm_flags)) {
		pr_err("CANNOT SLEEP DUE TO INFRA PDN BUT CPU PON\n");
		ret = -EPERM;
		goto LEAVE_SLEEP;
	}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	if (is_sspm_ipi_lock_spm()) {
		pr_err("CANNOT SLEEP DUE TO SSPM IPI\n");
		ret = -EPERM;
		goto LEAVE_SLEEP;
	}
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (!spm_load_firmware_status()) {
		pr_err("SPM FIRMWARE IS NOT READY\n");
		ret = -EPERM;
		goto LEAVE_SLEEP;
	}
#endif /* CONFIG_FPGA_EARLY_PORTING */

#if 1 /*removed : #ifdef CONFIG_MTK_ACAO_SUPPORT*/
	mcdi_task_pause(true);
#endif

#if SLP_SLEEP_DPIDLE_EN
#ifdef CONFIG_MTK_SND_SOC_NEW_ARCH
	if (slp_ck26m_on | fm_radio_is_playing)
#else
	if (slp_ck26m_on)
#endif /* CONFIG_MTK_SND_SOC_NEW_ARCH */
		slp_wake_reason = spm_go_to_sleep_dpidle(
				slp_spm_deepidle_flags,
				slp_spm_deepidle_flags1);
	else
#endif

		slp_wake_reason = spm_go_to_sleep(
				slp_spm_flags, slp_spm_flags1);
#if 1 /*removed : #ifdef CONFIG_MTK_ACAO_SUPPORT*/
	mcdi_task_pause(false);
#endif

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
	/* legacy log */
	/* slp_crit2("@@@@@@@\tChip_pm_finish\t@@@@@@@\n"); */
}

static void slp_suspend_ops_end(void)
{
	/* legacy log */
	/* slp_notice("@@@@@@@\tChip_pm_end\t@@@@@@@\n"); */
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
	pr_err("NO %s !!!\n", __func__);
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

	slp_notice("wakesrc = 0x%x, enable = %u, ck26m_on = %u\n",
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
		r = spm_set_dpidle_wakesrc(
				wakesrc & ~WAKE_SRC_CFG_KEY, enable, false);
	else
		r = spm_set_sleep_wakesrc(
				wakesrc & ~WAKE_SRC_CFG_KEY, enable, false);
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

void slp_set_infra_on(bool infra_on)
{
	if (infra_on) {
		slp_spm_flags |= SPM_FLAG_DIS_INFRA_PDN;
#if SLP_SLEEP_DPIDLE_EN
		slp_spm_deepidle_flags |= SPM_FLAG_DIS_INFRA_PDN;
#endif
	} else {
		slp_spm_flags &= ~SPM_FLAG_DIS_INFRA_PDN;
#if SLP_SLEEP_DPIDLE_EN
		slp_spm_deepidle_flags &= ~SPM_FLAG_DIS_INFRA_PDN;
#endif
	}
	slp_notice("slp set infra_on (%d): 0x%x, 0x%x\n",
			infra_on, slp_spm_flags, slp_spm_deepidle_flags);
}

void slp_module_init(void)
{
#if defined(CONFIG_MACH_MT6739) \
	|| defined(CONFIG_MACH_MT6763) \
	|| defined(CONFIG_MACH_MT6771)
	slp_suspend_ops_valid_on = true;
#else
	slp_suspend_ops_valid_on = false;
#endif

	spm_output_sleep_option();
	slp_notice("SLEEP_DPIDLE_EN:%d, REPLACE_DEF_WAKESRC:%d, SUSPEND_LOG_EN:%d\n",
		   SLP_SLEEP_DPIDLE_EN,
		   SLP_REPLACE_DEF_WAKESRC, SLP_SUSPEND_LOG_EN);
	suspend_set_ops(&slp_suspend_ops);
#if SLP_SUSPEND_LOG_EN
	console_suspend_enabled = 0;
#endif
}


/*
 * debugfs
 */
#define NR_CMD_BUF	128
static char dbg_buf[4096] = { 0 };
static char cmd_buf[512] = { 0 };
static struct dentry *spm_suspend_debugfs_file;

#undef mt_suspend_log
#define log2buf(p, s, fmt, args...) \
	(p += scnprintf(p, sizeof(s) - strlen(s), fmt, ##args))
#define mt_suspend_log(fmt, args...)	log2buf(p, dbg_buf, fmt, ##args)

static int _suspend_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int suspend_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _suspend_state_open, inode->i_private);
}

static ssize_t suspend_state_read(struct file *filp,
				  char __user *userbuf,
				  size_t count, loff_t *f_pos)
{
	int len = 0;
	char *p = dbg_buf;

	p[0] = '\0';
	mt_suspend_log("*********** suspend state ************\n");
	mt_suspend_log("suspend valid status = %d\n",
		       slp_suspend_ops_valid_on);
	mt_suspend_log("*********** suspend command ************\n");
	mt_suspend_log(
			"echo suspend 1/0 > /sys/kernel/debug/spm/suspend_state\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t suspend_state_write(struct file *filp,
				   const char __user *userbuf,
				   size_t count, loff_t *f_pos)
{
	char cmd[NR_CMD_BUF];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%127s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "suspend")) {
			/* update suspend valid status */
			slp_suspend_ops_valid_on = param;

			/* suspend reinit ops */
			suspend_set_ops(&slp_suspend_ops);
		}
		return count;
	}

	return -EINVAL;
}

static const struct file_operations suspend_state_fops = {
	.open = suspend_state_open,
	.read = suspend_state_read,
	.write = suspend_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

void spm_suspend_debugfs_init(struct dentry *spm_dir)
{
	spm_suspend_debugfs_file =
		debugfs_create_file("suspend_state",
				    0444,
				    spm_dir,
				    NULL,
				    &suspend_state_fops);
}

module_param(slp_ck26m_on, bool, 0644);
module_param(slp_spm_flags, uint, 0644);

module_param(slp_dump_gpio, bool, 0644);
module_param(slp_dump_golden_setting, bool, 0644);
module_param(slp_dump_golden_setting_type, int, 0644);

MODULE_DESCRIPTION("Sleep Driver v0.1");
