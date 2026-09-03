// SPDX-License-Identifier: GPL-2.0
//
// UNISOC APCPU POWER STAT driver
//
// Copyright (C) 2020 Unisoc, Inc.

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/device.h>
#include "sprd_pdbg_comm.h"
#include "sprd_regs_info.h"

#define SLP_FMT       "[SLP_STATE] deep: 0x%llx, light: 0x%llx\n"
#define EB_FMT        "[EB_INFO  ] ap1: 0x%llx, ap2: 0x%llx, aon1: 0x%llx, aon2: 0x%llx\n"
#define PD_FMT        "[PD_INFO  ] 0x%llx\n"
#define LPC_FMT       "[LPC_INFO ] 0x%llx\n"
#define DEEP_CNT_FMT  "[DEEP_CNT ] "
#define LIGHT_CNT_FMT "[LIGHT_CNT] "

static inline void sprd_pdbg_regs_msg_print(char *regs_msg, int *buf_cnt, bool print_out)
{
	if (print_out) {
		SPRD_PDBG_INFO("%s", regs_msg);
		*buf_cnt = 0;
	}
}

static int sprd_pdbg_regs_get(struct regs_info_data *regs_info, char *regs_msg, bool print_out)
{
	u64 slp_deep, slp_light, eb_ap1, eb_ap2, eb_aon1, eb_aon2, pd, lpc;
	u64 r_value[PDBG_INFO_NUM + 1] = {0};
	int buf_cnt = 0, cnt_num, cnt_low, cnt_high, cnt, i;
	char *pval;

	if (!regs_info) {
		SPRD_PDBG_ERR("%s: Parameter is error\n", __func__);
		return -EINVAL;
	}

	if (!sprd_pdbg_regs_get_once(PDBG_R_SLP, r_value)) {
		slp_deep = r_value[0];
		slp_light = r_value[1];
		buf_cnt += scnprintf(regs_msg + buf_cnt, REGS_LOG_BUF_MAX - buf_cnt,
				    SLP_FMT, slp_deep, slp_light);
		sprd_pdbg_regs_msg_print(regs_msg, &buf_cnt, print_out);
	}

	if (!sprd_pdbg_regs_get_once(PDBG_R_EB, r_value)) {
		eb_ap1 = r_value[0];
		eb_ap2 = r_value[1];
		eb_aon1 = r_value[2];
		eb_aon2 = r_value[3];
		buf_cnt += scnprintf(regs_msg + buf_cnt, REGS_LOG_BUF_MAX - buf_cnt,
				    EB_FMT, eb_ap1, eb_ap2, eb_aon1, eb_aon2);
		sprd_pdbg_regs_msg_print(regs_msg, &buf_cnt, print_out);
	}

	if (!sprd_pdbg_regs_get_once(PDBG_R_PD, r_value)) {
		pd = r_value[0];
		buf_cnt += scnprintf(regs_msg + buf_cnt, REGS_LOG_BUF_MAX - buf_cnt,
				    PD_FMT, pd);
		sprd_pdbg_regs_msg_print(regs_msg, &buf_cnt, print_out);
	}

	if (!sprd_pdbg_regs_get_once(PDBG_R_DCNT, r_value)) {
		pval = (char *)&r_value[4];
		pval -= 2;
		cnt_num = *pval;
		pval = (char *)&r_value[0];
		buf_cnt += scnprintf(regs_msg + buf_cnt, REGS_LOG_BUF_MAX - buf_cnt,
				    DEEP_CNT_FMT);
		for (i = 0; i < cnt_num; i++) {
			cnt_low = *(pval + 2*i);
			cnt_high = *(pval + 2*i + 1);
			cnt = (cnt_low | (cnt_high << 8));
			buf_cnt += scnprintf(regs_msg + buf_cnt, REGS_LOG_BUF_MAX - buf_cnt,
					    "%5d, ", cnt);
		}
		buf_cnt += scnprintf(regs_msg + buf_cnt, REGS_LOG_BUF_MAX - buf_cnt, "\n");
		sprd_pdbg_regs_msg_print(regs_msg, &buf_cnt, print_out);
	}

	if (!sprd_pdbg_regs_get_once(PDBG_R_LCNT, r_value)) {
		pval = (char *)&r_value[4];
		pval -= 2;
		cnt_num = *pval;
		pval = (char *)&r_value[0];
		buf_cnt += scnprintf(regs_msg + buf_cnt, REGS_LOG_BUF_MAX - buf_cnt,
				    LIGHT_CNT_FMT);
		for (i = 0; i < cnt_num; i++) {
			cnt_low = *(pval + 2*i);
			cnt_high = *(pval + 2*i + 1);
			cnt = (cnt_low | (cnt_high << 8));
			buf_cnt += scnprintf(regs_msg + buf_cnt, REGS_LOG_BUF_MAX - buf_cnt,
					    "%5d, ", cnt);
		}
		buf_cnt += scnprintf(regs_msg + buf_cnt, REGS_LOG_BUF_MAX - buf_cnt, "\n");
		sprd_pdbg_regs_msg_print(regs_msg, &buf_cnt, print_out);
	}

	if (!sprd_pdbg_regs_get_once(PDBG_R_LPC, r_value)) {
		lpc = r_value[0];
		if (lpc != PDBG_IGNORE_MAGIC) {
			buf_cnt += scnprintf(regs_msg + buf_cnt, REGS_LOG_BUF_MAX - buf_cnt,
					    LPC_FMT, lpc);
			sprd_pdbg_regs_msg_print(regs_msg, &buf_cnt, print_out);
		}
	}

	return 0;
}

static int regs_info_show(struct seq_file *seq, void *v)
{
	struct regs_info_data *regs_info = seq->private;

	mutex_lock(&regs_info->regs_info_mutex);
	sprd_pdbg_regs_get(regs_info, regs_info->log_buf, false);
	seq_printf(seq, "%s", regs_info->log_buf);
	mutex_unlock(&regs_info->regs_info_mutex);

	return 0;
}

static int sprd_regs_info_proc_init(struct regs_info_data *data, struct proc_dir_entry *dir)
{
	if (!proc_create_single_data("regs_info", 0644, dir, regs_info_show, data)) {
		SPRD_PDBG_ERR("%s: Proc regs_info file create failed\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static void sprd_pdbg_regs_info_show(struct regs_info_data *regs_info)
{
	sprd_pdbg_regs_get(regs_info, regs_info->log_buf, true);
}

static void sprd_pdbg_regs_info_show_locked(struct regs_info_data *regs_info)
{
	mutex_lock(&regs_info->regs_info_mutex);
	sprd_pdbg_regs_get(regs_info, regs_info->log_buf, true);
	mutex_unlock(&regs_info->regs_info_mutex);
}

static void regs_info_notify_handler(void *data, unsigned long cmd)
{
	struct regs_info_data *regs_info = data;

	switch (cmd) {
	case SPRD_CPU_PM_ENTER:
		sprd_pdbg_regs_info_show(regs_info);
		break;
	case SPRD_PM_MONITOR:
		sprd_pdbg_regs_info_show_locked(regs_info);
		break;
	default:
		break;
	}
}

static void regs_info_devm_action(void *_data)
{
	struct regs_info_data *regs_info = _data;

	mutex_destroy(&regs_info->regs_info_mutex);
}

int sprd_regs_info_init(struct device *dev, struct proc_dir_entry *dir,
			struct regs_info_data **data)
{
	int ret;
	struct regs_info_data *regs_info;

	regs_info = devm_kzalloc(dev, sizeof(struct regs_info_data), GFP_KERNEL);
	if (!regs_info) {
		SPRD_PDBG_ERR("%s: regs_info alloc error\n", __func__);
		return -ENOMEM;
	}

	regs_info->notify_cb = regs_info_notify_handler;
	mutex_init(&regs_info->regs_info_mutex);

	ret = devm_add_action(dev, regs_info_devm_action, regs_info);
	if (ret) {
		regs_info_devm_action(regs_info);
		SPRD_PDBG_ERR("failed to add regs_info devm action\n");
		return ret;
	}

	ret = sprd_regs_info_proc_init(regs_info, dir);
	if (ret) {
		SPRD_PDBG_ERR("%s: sprd_regs_info_proc_init error\n", __func__);
		return ret;
	}

	*data = regs_info;

	return 0;
}

