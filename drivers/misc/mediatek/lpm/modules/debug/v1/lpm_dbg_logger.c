// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/rtc.h>
#include <linux/wakeup_reason.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/ctype.h>

#include <lpm.h>

#include <mtk_lpm_sysfs.h>
#include <mtk_cpupm_dbg.h>
#include <lpm_dbg_common_v1.h>
#include <lpm_timer.h>
#include <lpm_module.h>

#if IS_ENABLED(CONFIG_MTK_LPM_MT6983)
#include <lpm_dbg_cpc_v5.h>
#else
#include <lpm_dbg_cpc_v3.h>
#endif

#include <lpm_dbg_syssram_v1.h>

#define LPM_LOG_DEFAULT_MS		5000

#define PCM_32K_TICKS_PER_SEC		(32768)
#define PCM_TICK_TO_SEC(TICK)	(TICK / PCM_32K_TICKS_PER_SEC)

#define aee_sram_printk pr_info

#define TO_UPPERCASE(Str) ({ \
	char buf_##Cnt[sizeof(Str)+4]; \
	char *str_##Cnt = Str; \
	int ix_##Cnt = 0; \
	for (; *str_##Cnt; str_##Cnt++, ix_##Cnt++) \
		if (ix_##Cnt < sizeof(buf_##Cnt)-1) \
			buf_##Cnt[ix_##Cnt] = toupper(*str_##Cnt); \
	buf_##Cnt; })

//static struct lpm_spm_wake_status lpm_wake;
static struct lpm_dbg_plat_ops _lpm_dbg_plat_ops;

void __iomem *lpm_spm_base;
EXPORT_SYMBOL(lpm_spm_base);

struct lpm_log_helper lpm_logger_help = {
	//.wakesrc = &lpm_wake,
	.wakesrc = NULL,
	.cur = 0,
	.prev = 0,
};

struct lpm_logger_timer {
	struct lpm_timer tm;
	unsigned int fired;
};

struct lpm_logger_fired_info {
	unsigned int fired;
	unsigned long logger_en_state;
	char **state_name;
	int fired_index;
	unsigned int mcusys_cnt_chk;
};

static struct lpm_logger_timer lpm_log_timer;
static struct lpm_logger_fired_info lpm_logger_fired;

static void lpm_check_cg_pll(void)
{
	int i;
	u32 block;
	u32 blkcg;

	block = (u32)
		lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RC_DUMP_PLL,
				MT_LPM_SMC_ACT_GET, 0, 0);
	if (block != 0) {
		for (i = 0 ; i < spm_cond.pll_cnt ; i++) {
			if (block & 1 << (16+i))
				pr_info("suspend warning: pll: %s not closed\n"
					, spm_cond.pll_str[i]);
		}
	}

	/* Definition about SPM_COND_CHECK_BLOCKED
	 * bit [00 ~ 15]: cg blocking index
	 * bit [16 ~ 29]: pll blocking index
	 * bit [30]     : pll blocking information
	 * bit [31]	: idle condition check fail
	 */

	for (i = 1 ; i < spm_cond.cg_cnt ; i++) {
		blkcg = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_BLOCK_DETAIL, MT_LPM_SMC_ACT_GET, 0, i);
		if (blkcg != 0)
			pr_info("suspend warning: CG: %6s = 0x%08x\n"
				, spm_cond.cg_str[i], blkcg);
	}

}

int lpm_dbg_plat_ops_register(struct lpm_dbg_plat_ops *lpm_dbg_plat_ops)
{
	if (!lpm_dbg_plat_ops)
		return -1;

	_lpm_dbg_plat_ops.lpm_show_message = lpm_dbg_plat_ops->lpm_show_message;
	_lpm_dbg_plat_ops.lpm_save_sleep_info = lpm_dbg_plat_ops->lpm_save_sleep_info;
	_lpm_dbg_plat_ops.lpm_get_spm_wakesrc_irq = lpm_dbg_plat_ops->lpm_get_spm_wakesrc_irq;
	_lpm_dbg_plat_ops.lpm_get_wakeup_status = lpm_dbg_plat_ops->lpm_get_wakeup_status;
	return 0;
}
EXPORT_SYMBOL(lpm_dbg_plat_ops_register);


int lpm_issuer_func(int type, const char *prefix, void *data)
{
	if (!_lpm_dbg_plat_ops.lpm_get_wakeup_status)
		return -1;

	_lpm_dbg_plat_ops.lpm_get_wakeup_status();

	if (type == LPM_ISSUER_SUSPEND)
		lpm_check_cg_pll();

	if (_lpm_dbg_plat_ops.lpm_show_message)
		return _lpm_dbg_plat_ops.lpm_show_message(
			 type, prefix, data);
	else
		return -1;
}

struct lpm_issuer issuer = {
	.log = lpm_issuer_func,
	.log_type = 0,
};

static int lpm_idle_save_sleep_info_nb_func(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct lpm_nb_data *nb_data = (struct lpm_nb_data *)data;

	if (nb_data && (action == LPM_NB_BEFORE_REFLECT))
		if (_lpm_dbg_plat_ops.lpm_save_sleep_info)
			_lpm_dbg_plat_ops.lpm_save_sleep_info();

	return NOTIFY_OK;
}

struct notifier_block lpm_idle_save_sleep_info_nb = {
	.notifier_call = lpm_idle_save_sleep_info_nb_func,
};

static void lpm_suspend_save_sleep_info_func(void)
{
	if (_lpm_dbg_plat_ops.lpm_save_sleep_info)
		_lpm_dbg_plat_ops.lpm_save_sleep_info();
}

static struct syscore_ops lpm_suspend_save_sleep_info_syscore_ops = {
	.resume = lpm_suspend_save_sleep_info_func,
};

static int lpm_log_timer_func(unsigned long long dur, void *priv)
{
	struct lpm_logger_timer *timer =
			(struct lpm_logger_timer *)priv;
	struct lpm_logger_fired_info *info = &lpm_logger_fired;
	static unsigned int mcusys_cnt_prev, mcusys_cnt_cur;
	char wakeup_sources[MAX_SUSPEND_ABORT_LEN];

	if (timer->fired != info->fired) {
		if (issuer.log_type >= LOG_SUCCEESS &&
				info->mcusys_cnt_chk == 1) {
			mcusys_cnt_cur =
			mtk_cpupm_syssram_read(SYSRAM_MCUSYS_CNT) +
			mtk_cpupm_syssram_read(SYSRAM_RECENT_MCUSYS_CNT);

			if (mcusys_cnt_prev == mcusys_cnt_cur)
				issuer.log_type = LOG_MCUSYS_NOT_OFF;

			mcusys_cnt_prev = mcusys_cnt_cur;
		}

		issuer.log(LPM_ISSUER_CPUIDLE,
			info->state_name[info->fired_index], (void *)&issuer);
	} else {
		pr_info("[name:spm&][SPM] %s didn't enter low power scenario\n",
			info->state_name && info->state_name[info->fired_index] ?
			info->state_name[info->fired_index] :
			"LPM");
	}

	pm_get_active_wakeup_sources(wakeup_sources, MAX_SUSPEND_ABORT_LEN);
	pr_info("[name:spm&] %s\n", wakeup_sources);

	timer->fired = info->fired;
	return 0;
}

static int lpm_logger_nb_func(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct lpm_nb_data *nb_data = (struct lpm_nb_data *)data;
	struct lpm_logger_fired_info *info = &lpm_logger_fired;
	unsigned long tar_state = (1 << nb_data->index);

	if (nb_data && (action == LPM_NB_BEFORE_REFLECT)
		&& (tar_state & (info->logger_en_state))) {
		issuer.log_type = nb_data->ret;
		info->fired++;
		info->fired_index = nb_data->index;
	}

	return NOTIFY_OK;
}

struct notifier_block lpm_logger_nb = {
	.notifier_call = lpm_logger_nb_func,
};

static ssize_t lpm_logger_debugfs_read(char *ToUserBuf,
					size_t sz, void *priv)
{
	char *p = ToUserBuf;
	int len;

	if (priv == ((void *)&lpm_log_timer)) {
		len = scnprintf(p, sz, "%lu\n",
			lpm_timer_interval(&lpm_log_timer.tm));
		p += len;
	}

	return (p - ToUserBuf);
}

static ssize_t lpm_logger_debugfs_write(char *FromUserBuf,
				   size_t sz, void *priv)
{
	if (priv == ((void *)&lpm_log_timer)) {
		unsigned int val = 0;

		if (!kstrtouint(FromUserBuf, 10, &val)) {
			if (val == 0)
				lpm_timer_stop(&lpm_log_timer.tm);
			else
				lpm_timer_interval_update(
						&lpm_log_timer.tm, val);
		}
	}

	return sz;
}

struct LPM_LOGGER_NODE {
	struct mtk_lp_sysfs_handle handle;
	struct mtk_lp_sysfs_op op;
};
#define LPM_LOGGER_NODE_INIT(_n, _priv) ({\
	_n.op.fs_read = lpm_logger_debugfs_read;\
	_n.op.fs_write = lpm_logger_debugfs_write;\
	_n.op.priv = _priv; })\


struct mtk_lp_sysfs_handle lpm_log_tm_node;
struct LPM_LOGGER_NODE lpm_log_tm_interval;

int lpm_logger_timer_debugfs_init(void)
{
	mtk_lpm_sysfs_sub_entry_add("logger", 0644,
				NULL, &lpm_log_tm_node);

	LPM_LOGGER_NODE_INIT(lpm_log_tm_interval,
				&lpm_log_timer);
	mtk_lpm_sysfs_sub_entry_node_add("interval", 0644,
				&lpm_log_tm_interval.op,
				&lpm_log_tm_node,
				&lpm_log_tm_interval.handle);
	return 0;
}

int lpm_logger_init(void)
{
	struct device_node *node = NULL;
	struct cpuidle_driver *drv = NULL;
	struct property *prop;
	const char *logger_enable_name;
	struct lpm_logger_fired_info *info = &lpm_logger_fired;
	int ret = 0, idx = 0, state_cnt = 0;


	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");

	if (node) {
		lpm_spm_base = of_iomap(node, 0);
		of_node_put(node);
	}

	if (lpm_spm_base)
		lpm_issuer_register(&issuer);
	else
		pr_info("[name:mtk_lpm][P] - Don't register the issue by error! (%s:%d)\n",
			__func__, __LINE__);

	if (_lpm_dbg_plat_ops.lpm_get_spm_wakesrc_irq)
		_lpm_dbg_plat_ops.lpm_get_spm_wakesrc_irq();

	mtk_lpm_sysfs_root_entry_create();
	lpm_logger_timer_debugfs_init();

	drv = cpuidle_get_driver();

	info->logger_en_state = 0;

	info->mcusys_cnt_chk = 0;

	node = of_find_compatible_node(NULL, NULL,
					MTK_LPM_DTS_COMPATIBLE);

	if (drv && node) {
		state_cnt = of_property_count_strings(node,
				"logger-enable-states");
		if (state_cnt)
			info->state_name =
			kcalloc(1, sizeof(char *)*(drv->state_count),
			GFP_KERNEL);

		if (!info->state_name)
			return -ENOMEM;

		for (idx = 0; idx < drv->state_count; idx++) {

			if (!state_cnt) {
				pr_info("[%s:%d] no logger-enable-states\n",
						__func__, __LINE__);
				break;
			}

			of_property_for_each_string(node,
					"logger-enable-states",
					prop, logger_enable_name) {
				if (!strcmp(logger_enable_name,
						drv->states[idx].name)) {
					info->logger_en_state |= (1 << idx);

					info->state_name[idx] =
					kcalloc(1, sizeof(char)*
					(strlen(drv->states[idx].name) + 1),
					GFP_KERNEL);

					if (!info->state_name[idx]) {
						kfree(info->state_name);
						return -ENOMEM;
					}

					strncat(info->state_name[idx],
					drv->states[idx].name,
					strlen(drv->states[idx].name));
				}
			}
		}

		of_property_read_u32(node, "mcusys-cnt-chk",
					&info->mcusys_cnt_chk);
	}

	if (node)
		of_node_put(node);

	lpm_notifier_register(&lpm_logger_nb);
	lpm_notifier_register(&lpm_idle_save_sleep_info_nb);

	lpm_log_timer.tm.timeout = lpm_log_timer_func;
	lpm_log_timer.tm.priv = &lpm_log_timer;
	lpm_timer_init(&lpm_log_timer.tm, LPM_TIMER_REPEAT);
	lpm_timer_interval_update(&lpm_log_timer.tm,
					LPM_LOG_DEFAULT_MS);
	lpm_timer_start(&lpm_log_timer.tm);

	ret = spm_cond_init();
	if (ret)
		pr_info("[%s:%d] - spm_cond_init failed\n",
			__func__, __LINE__);

	register_syscore_ops(&lpm_suspend_save_sleep_info_syscore_ops);

	return 0;
}
EXPORT_SYMBOL(lpm_logger_init);

void lpm_logger_deinit(void)
{
	struct device_node *node = NULL;
	int state_cnt = 0;
	int idx = 0;
	struct lpm_logger_fired_info *info = &lpm_logger_fired;

	spm_cond_deinit();

	node = of_find_compatible_node(NULL, NULL,
					MTK_LPM_DTS_COMPATIBLE);

	if (node)
		state_cnt = of_property_count_strings(node,
			"logger-enable-states");

	if (state_cnt && info) {
		for_each_set_bit(idx, &info->logger_en_state, 32)
			kfree(info->state_name[idx]);
		kfree(info->state_name);
	}

	if (node)
		of_node_put(node);
}
EXPORT_SYMBOL(lpm_logger_deinit);
