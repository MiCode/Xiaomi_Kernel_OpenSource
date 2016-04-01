/*
 * Atmel maXTouch Touchscreen driver Plug in
 *
 * Copyright (C) 2013 Atmel Co.Ltd
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Author: Pitter Liao <pitter.liao@atmel.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

/****************************************************************
	Pitter Liao add for macro for the global platform
		email:  pitter.liao@atmel.com
		mobile: 13244776877
-----------------------------------------------------------------*/
#define PLUG_MISC_VERSION 0x0012
/*----------------------------------------------------------------
fixed some bugs
0.12
1 PSD store , ZERO UID check
0.11
1 Fixed PTC tuning issue when bootup and first tuning
0.1
1 first version of msc plugin: PTC/PID support
*/

#include "plug.h"
#include <linux/delay.h>

#define MSC_FLAG_RESETING				(1<<0)
#define MSC_FLAG_CALING					(1<<1)

#define MSC_FLAG_RESET					(1<<4)
#define MSC_FLAG_CAL						(1<<5)
#define MSC_FLAG_RESUME					(1<<6)

#define MSC_FLAG_PSD_UPDATING				(1<<12)
#define MSC_FLAG_PSD_UPDATED				(1<<13)
#define MSC_FLAG_PSD_FAILED    				(1<<14)

#define MSC_FLAG_PSD_VALID					(1<<16)

#define MSC_FLAG_PTC_MASK_SHIFT			20

#define MSC_FLAG_PTC_TUNING					(1<<20)
#define MSC_FLAG_PTC_TUNED					(1<<21)
#define MSC_FLAG_PTC_FAILED					(1<<22)

#define MSC_FLAG_PTC_PARA_VALID				(1<<24)
#define MSC_FLAG_PTC_INITED					(1<<25)

#define MSC_FLAG_WORKAROUND_HALT			(1<<31)

#define MSC_FLAG_MASK_LOW			(0x00f0)
#define MSC_FLAG_MASK_NORMAL		(0xf00)
#define MSC_FLAG_MASK_PSD			(0xf000)
#define MSC_FLAG_MASK_PID			(0xf0000)
#define MSC_FLAG_MASK_PTC			(0xf00000)
#define MSC_FLAG_MASK				(-1)

#define MSC_STEP_PTC_SET_PARA			(1<<1)
#define MSC_STEP_PTC_IDLE				(1<<2)

#define MSC_STEP_PTC_MASK				(0xf)

char tp_lockdown_info_atmel[128];

enum {
	LIMIT_PARAM_MIN = 0,
	LIMIT_PARAM_MAX,
	LIMIT_STEP_MAX,
	LIMIT_COUNT_MAX,
	LIMIT_REF_MIN,
	LIMIT_REF_MAX,
	NUM_LIMIT,
};

enum {
	P_PARAM = 0,
	P_PARAM_B,
	P_REF,
	P_REF_B,
	P_STEP,
	P_COUNT,
	NUM_PTC_STATUS,
};

static const char *ptc_para_name[] = {
	"P_PARAM",
	"P_PARAM_B",
	"P_REF",
	"P_REF_B",
	"P_STEP",
	"P_COUNT",
};

struct psd_code{
	u8 head[MISC_PDS_HEAD_LEN];
	u8 id[MISC_PDS_PID_LEN];
	u8  params[MISC_PDS_PTC_LEN];
};

struct psd_observer {
	struct psd_code code;
	unsigned long flag;
	u8 *frame_buf;
	u8 frame_size;
	u8 frame_ofs;

	struct t68_config_head *head;
	struct t68_config_tail *tail;
};

struct pid_name{
	char *name;
	u8 id[MISC_PDS_PID_LEN];
};

struct pid_observer {
	u8 rsv;
};

struct ptc_status {
	s16 para[NUM_PTC_STATUS][PTC_KEY_GROUPS];
#define PTC_STATUS_SIGNAL_CHANGED  (1<<0)
#define PTC_STATUS_SEARCH_BEST_SIGNAL  (1<<1)
	unsigned long flag;
	unsigned long step;
} __packed;

struct ptc_observer {
	struct ptc_status status;
};

struct msc_observer{
	unsigned long flag;

	struct psd_observer psd;
	struct pid_observer pid;
	struct ptc_observer ptc;
};

struct psd_config {
	u16 reg_len;
	u16 data_len;
};

struct pid_config {
	const struct pid_name *name_list;
	int num_name;
};

struct ptc_limit{
	s16 *param;
	int num_param;
};

struct ptc_config {
	struct ptc_status init;
	struct ptc_limit limit;
	s16 target;
	s16 max_count;
};

struct msc_config{
	struct psd_config psd;
	struct pid_config pid;
	struct ptc_config ptc;
};

static ssize_t misc_ptc_show(struct plugin_misc *p, char *buf, size_t count);
static int msc_set_spare_data(struct plugin_misc *p, struct psd_observer *psd_obs, u8 *buf, u8 len, unsigned long flag);

static void plugin_misc_hook_t6(struct plugin_misc *p, u8 status)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct msc_observer *obs = p->obs;

	if (status & (MXT_T6_STATUS_RESET|MXT_T6_STATUS_CAL)) {
		dev_dbg2(dev, "MSC hook T6 0x%x\n", status);

		if (status & MXT_T6_STATUS_CAL) {
			set_and_clr_flag(MSC_FLAG_CALING,
				0, &obs->flag);
		}

		if (status & MXT_T6_STATUS_RESET) {
			set_and_clr_flag(MSC_FLAG_RESETING,
				MSC_FLAG_MASK_NORMAL, &obs->flag);
		}
	} else {
		if (test_flag(MSC_FLAG_RESETING, &obs->flag))
			set_and_clr_flag(MSC_FLAG_RESET,
				MSC_FLAG_RESETING, &obs->flag);
		if (test_flag(MSC_FLAG_CALING, &obs->flag))
			set_and_clr_flag(MSC_FLAG_CAL,
				MSC_FLAG_CALING, &obs->flag);

			dev_dbg2(dev, "MSC hook T6 end\n");
	}

}

static void plugin_misc_hook_t68(struct plugin_misc *p, u8 *msg)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct msc_observer *obs = p->obs;
	struct psd_observer *psd_obs = &obs->psd;
	int state = msg[1];
	int ret;

	dev_info2(dev, "mxt hook pi t68 0x%x\n",
		state);

	if (state == 0) {
		dev_info(dev, "T68 write data %d\n", psd_obs->frame_ofs);
		if (test_flag(MSC_FLAG_PSD_UPDATING, &obs->flag) &&
				test_flag(MSC_FLAG_PSD_VALID, &obs->flag)) {
			ret = msc_set_spare_data(p, psd_obs, psd_obs->code.id,
				sizeof(psd_obs->code.id) + sizeof(psd_obs->code.params), 0);
			if (ret == 0)
				set_and_clr_flag(MSC_FLAG_PSD_UPDATED, MSC_FLAG_MASK_PSD, &obs->flag);
			else if (ret && ret != -EAGAIN)
				dev_err(dev, "msc_set_spare_data failed %d len %d(%d)\n", ret, psd_obs->frame_size, psd_obs->frame_ofs);
		}
	} else {
		dev_err(dev, "T68 failed state = %d\n",
			state);
		if (test_flag(MSC_FLAG_PSD_UPDATING, &obs->flag))
			set_and_clr_flag(MSC_FLAG_PSD_FAILED, MSC_FLAG_MASK_PSD, &obs->flag);
	}
}

static int msc_psd_get_data(struct plugin_misc *p, struct psd_code *code)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;

	int ret = -EINVAL;

	int retry = 3;

	if (!code)
		return ret;

	if (p->get_diagnostic_data) {
		do {
			ret = p->get_diagnostic_data(p->dev, MXT_T6_DEBUG_PID, 0, 0, MISC_PDS_HEAD_LEN, (char *)&code->head[0], 10, 5);
			if (ret == 0)  {
				dev_info2(dev, "Found PID head %02x %02x.\n", code->head[0], code->head[1]);
				if (code->head[0] == PID_MAGIC_WORD0 && code->head[1] == PID_MAGIC_WORD1) {
					ret = p->get_diagnostic_data(p->dev, MXT_T6_DEBUG_PID, 0, MISC_PDS_HEAD_LEN, MISC_PDS_DATA_LEN, (char *)&code->id[0], 10, 5);
					if (ret == 0)
						break;
				}
			}
			msleep(10);
		} while (--retry);

		if (!retry)
			ret = -ENODATA;

		if (ret == 0)
		dev_info2(dev, "Found atmel PID %02x%02x%02x%02x ...\n", code->id[0], code->id[1], code->id[2], code->id[3]);


		else {
			dev_err(dev, "Read PID failed %d\n", ret);
			memset(code, 0, sizeof(struct psd_code));
		}
	}

	return ret;
}

static int msc_ptc_get_data(struct plugin_misc *p, s16 *val, int num)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;

	int ret = -EINVAL;

	int retry = 3;

	if (!val)
		return ret;

	if (p->get_diagnostic_data) {
		do {
			ret = p->get_diagnostic_data(p->dev, MXT_T6_DEBUG_REF_PTC, 0, 0, num * sizeof(*val), (char *)val, 10, 5);
			if (ret == 0)
				break;

			msleep(10);
		} while (--retry);

		if (!retry)
			ret = -ENODATA;

		if (ret == 0)
			dev_info2(dev, "PTC Ref %hd %hd %hd %hd\n", val[0], val[1], val[2], val[3]);
		else {
			dev_err(dev, "Red PTC Ref failed %d\n", ret);
			memset(val, 0, num * sizeof(*val));
		}
	}

	return ret;
}

/*
	Algorithm:
	Ref > Target:   [do add]
	    step > 0: step <<= 1    [last add, double it]
	    step < 0: step = -(step >>1)  [last sub, half it and change signal]
	Ref < Target:   [do sub]
	    step > 0: step = -(step >> 1) [last inc, half it and change signal]
	    step < 0: step <<= 1  [last sub, double it]
*/
static int ptc_set_next_param(struct ptc_status *s1, const struct ptc_config *cfg)
{
	s16 signal, signal_b;
	s32 step;
	int i, ret = 0;

	for (i = 0; i < PTC_KEY_GROUPS; i++) {
		if (s1->para[P_PARAM][i]) {
			signal = cfg->target - s1->para[P_REF][i];
			if (!test_and_set_flag(PTC_STATUS_SEARCH_BEST_SIGNAL, &s1->flag)) {
				s1->para[P_REF_B][i] = s1->para[P_REF][i];
				s1->para[P_PARAM_B][i] = s1->para[P_PARAM][i];
			} else {
				signal_b = cfg->target - s1->para[P_REF_B][i];
				if (abs(signal) < abs(signal_b)) {
					s1->para[P_REF_B][i] = s1->para[P_REF][i];
					s1->para[P_PARAM_B][i] = s1->para[P_PARAM][i];
				}
			}

			if (s1->para[P_STEP][i] && s1->para[P_COUNT][i] < cfg->limit.param[LIMIT_COUNT_MAX]) {
				if (signal) {
					signal ^= s1->para[P_STEP][i];
					step = s1->para[P_STEP][i];
					if (signal > 0) {
						if (s1->para[P_COUNT][i]) {
							step = (s1->para[P_STEP][i] >> 1);
							set_flag(PTC_STATUS_SIGNAL_CHANGED, &s1->flag);
						}
						s1->para[P_STEP][i] = -(s16)step;
						if (s1->para[P_STEP][i])
							ret = -EAGAIN;
					} else {
						if (!test_flag(PTC_STATUS_SIGNAL_CHANGED, &s1->flag)) {
							if (s1->para[P_STEP][i] < cfg->limit.param[LIMIT_PARAM_MAX]) {
								if (s1->para[P_COUNT][i])
									step = s1->para[P_STEP][i] + (s1->para[P_STEP][i] >> 1);
								if (step > cfg->limit.param[LIMIT_STEP_MAX])
									step = cfg->limit.param[LIMIT_STEP_MAX];
								if (step < -cfg->limit.param[LIMIT_STEP_MAX])
									step = -cfg->limit.param[LIMIT_STEP_MAX];
								s1->para[P_STEP][i] = (s16)step;
							}
						}
						ret = -EAGAIN;
					}
				}
				s1->para[P_COUNT][i]++;
			}
		}
	}

	return ret;
}

static int ptc_check_param_limit(struct ptc_status *s1, const struct ptc_config *cfg)
{
	int i;
	int ret = -ERANGE;
	s32 param;

	for (i = 0; i < PTC_KEY_GROUPS; i++) {
		if (s1->para[P_PARAM][i] && s1->para[P_STEP][i] && s1->para[P_COUNT][i]  < cfg->limit.param[LIMIT_COUNT_MAX]) {
			param = s1->para[P_PARAM][i] + s1->para[P_STEP][i];
			if (param > cfg->limit.param[LIMIT_PARAM_MAX])
				param = cfg->limit.param[LIMIT_PARAM_MAX];
			if (param < cfg->limit.param[LIMIT_PARAM_MIN])
				param = cfg->limit.param[LIMIT_PARAM_MIN];
			if ((s1->para[P_PARAM][i] > cfg->limit.param[LIMIT_PARAM_MIN] &&	s1->para[P_PARAM][i] < cfg->limit.param[LIMIT_PARAM_MAX]) ||
				(param > cfg->limit.param[LIMIT_PARAM_MIN] && param < cfg->limit.param[LIMIT_PARAM_MAX])) {
				s1->para[P_PARAM][i] = (s16)param;
				ret = 0;
			}
		}
	}

	return ret;
}

static int ptc_params_valid(const struct ptc_config *ptc_cfg, const struct ptc_status *s1, int chk, int lo, int hi)
{
	int ret = 0;
	int i;

	if (chk >= NUM_PTC_STATUS)
		return -EINVAL;

	if (lo >= NUM_LIMIT || hi >= NUM_LIMIT)
		return -EINVAL;

	for (i = 0; i < PTC_KEY_GROUPS; i++) {
		if (ptc_cfg->init.para[chk][i]) {
			if (s1->para[chk][i] < ptc_cfg->limit.param[lo] ||
				s1->para[chk][i] > ptc_cfg->limit.param[hi]) {
				ret = -ERANGE;
				break;
			}
		} else {
			if (s1->para[chk][i]) {
				ret = -ERANGE;
				break;
			}
		}
	}

	return ret;
}

static int msc_ptc_set_param(struct plugin_misc *p, s16 *params, int num)
{
	int count, size, sum, ret;

	struct reg_config t96_config = {.reg = MXT_TOUCH_SPT_PTC_TUNINGPARAMS_T96, 0,
			.offset = 0, .buf = {0x0}, .len = 8, .mask = 0, .flag = 0};

	sum = num * sizeof(*params);
	size = 0;
	do {
		count = sum;
		if (count > sizeof(t96_config.buf))
			count = sizeof(t96_config.buf);
		memcpy(t96_config.buf, &params[size], count);
		sum -= count;
		size += count;

		ret = p->set_obj_cfg(p->dev, &t96_config, NULL, 0);
		if (ret)
			break;
	} while (sum > 0);

	return ret;
}

#define SET_PSD_DATA_START (1<<0)
#define SET_PSD_DATA_CONTINUE (1<<0)
static int msc_set_spare_data(struct plugin_misc *p, struct psd_observer *psd_obs, u8 *buf, u8 len, unsigned long flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct msc_config *cfg = p->cfg;
	struct psd_config *psd_cfg = &cfg->psd;
	int data_size, ret;

	struct reg_config rcfg = {.reg = MXT_SPARE_T68, 0,
			.offset = 0, .buf = {0}, .len = 0, .mask = 0, .flag = 0};

	if (!psd_obs->frame_buf)
		return -ENOMEM;

	psd_obs->head->ctrl = 0x3;
	psd_obs->head->type = 5;
	if (test_flag(SET_PSD_DATA_START, &flag)) {
		psd_obs->frame_size = len;
		psd_obs->frame_ofs = 0;
		psd_obs->tail->cmd = 0x1;
	} else {
		if (psd_obs->frame_ofs < psd_obs->frame_size)
			psd_obs->tail->cmd = 2;
		else
			psd_obs->tail->cmd = 3;
	}

	data_size = psd_obs->frame_size - psd_obs->frame_ofs;
	if (data_size > psd_cfg->data_len)
		data_size = psd_cfg->data_len;
	psd_obs->head->len = data_size;

	if (data_size)
		memcpy(psd_obs->head + 1, buf + psd_obs->frame_ofs, data_size);

	rcfg.len = psd_cfg->reg_len;
	rcfg.ext_buf = (u8 *)psd_obs->head;
	ret = p->set_obj_cfg(p->dev, &rcfg, NULL, FLAG_REG_DATA_IN_EXT_BUF);
	if (ret == 0) {
		psd_obs->frame_ofs += data_size;
		if (psd_obs->frame_ofs < psd_obs->frame_size)
			ret = -EAGAIN;
	} else {
		dev_err(dev, "Failed write T68 start frame failed %d(len %d)\n", ret, rcfg.len);
	}

	return ret;
}

#define PTC_STATUS_REINIT  (1<<0)
#define PTC_STATUS_CONTINUE  (1<<1)
static void msc_ptc_reset_param(struct plugin_misc *p, struct ptc_status *sts, unsigned long flag)
{
	struct msc_config *cfg = p->cfg;
	struct ptc_config *ptc_cfg = &cfg->ptc;

	if (test_flag(PTC_STATUS_CONTINUE, &flag)) {
		memcpy(sts->para[P_STEP], &ptc_cfg->init.para[P_STEP], sizeof(sts->para[P_STEP]));
		memset(sts->para[P_COUNT], 0, sizeof(sts->para[P_COUNT]));
		memset(sts->para[P_PARAM_B], 0, sizeof(sts->para[P_PARAM_B]));
		memset(sts->para[P_REF_B], 0, sizeof(sts->para[P_REF_B]));
		clear_flag(-1, &sts->flag);
	} else
		memcpy(sts, &ptc_cfg->init, sizeof(*sts));
}

static int msc_ptc_tune_process(struct plugin_misc *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct msc_observer *obs = p->obs;
	struct msc_config *cfg = p->cfg;
	struct ptc_config *ptc_cfg = &cfg->ptc;
	struct ptc_observer *ptc_obs = &obs->ptc;
	struct ptc_status *sts = &ptc_obs->status;
	int ret;

	ret = msc_ptc_get_data(p, sts->para[P_REF], PTC_KEY_GROUPS);
	if (ret)
		return ret;

	misc_ptc_show(p, NULL, 0);
	ret = ptc_set_next_param(sts, ptc_cfg);
	if (ret == 0) {
		dev_info2(dev, "mxt msc PTC tune finished\n");
		misc_ptc_show(p, NULL, 0);
	} else {
		ret = ptc_check_param_limit(sts, ptc_cfg);
		if (ret == 0) {
			ret = msc_ptc_set_param(p, sts->para[P_PARAM], PTC_KEY_GROUPS);
		}
		if (ret == 0)
			ret = -EAGAIN;
	}
	return ret;
}

static ssize_t misc_ptc_show(struct plugin_misc *p, char *buf, size_t count)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct msc_config *cfg = p->cfg;
	struct msc_observer *obs = p->obs;
	struct ptc_config *ptc_cfg = &cfg->ptc;
	struct ptc_observer *ptc_obs = &obs->ptc;
	struct ptc_status *sts = &ptc_obs->status;
	int offset = 0;
	int ret;
	int i;

	if (!p->init)
		return 0;

	dev_info(dev, "[mxt]PTC status: Flag=0x%08lx Step=0x%08lx\n",
		sts->flag, sts->step);

	print_dec16_buf(KERN_INFO, "PTC Limit", ptc_cfg->limit.param, ptc_cfg->limit.num_param);

	for (i = 0; i < NUM_PTC_STATUS; i++)
		print_dec16_buf(KERN_INFO, ptc_para_name[i], &sts->para[i][0], PTC_KEY_GROUPS);

	if (count > 0) {
		ret = ptc_params_valid(ptc_cfg, sts, P_REF, LIMIT_REF_MIN, LIMIT_REF_MAX);
		i = (ret == 0) ? 1 : 0;
		offset += scnprintf(buf, count, "PTC: %d\n", i);
		offset += dec_dump_to_buffer("PTC Limit", ptc_cfg->limit.param, ptc_cfg->limit.num_param, 2, buf + offset, count - offset);
		for (i = 0; i < NUM_PTC_STATUS; i++)
			offset += dec_dump_to_buffer(ptc_para_name[i], &sts->para[i][0], PTC_KEY_GROUPS, 2, buf + offset, count - offset);
	}

	return 0;
}

static ssize_t misc_psd_show(struct plugin_misc *p, char *buf, size_t count)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct msc_observer *obs = p->obs;
	struct psd_observer *psd_obs = &obs->psd;
	int offset = 0;

	if (!p->init)
		return 0;

	dev_info(dev, "[mxt]misc status: Flag=0x%08lx\n",
		psd_obs->flag);

	print_hex_dump(KERN_INFO, "[mxt] PID: ", DUMP_PREFIX_NONE, 16, 1,
			&obs->psd.code, sizeof(struct psd_code), false);

	if (count > 0) {

		offset += dec_dump_to_buffer(NULL, obs->psd.code.id, sizeof(obs->psd.code.id), 1, buf, count - offset);
	}

	return offset;
}

static int plugin_misc_get_pid_name(struct plugin_misc *p, char *name, int len)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct msc_observer *obs = p->obs;
	struct msc_config *cfg = p->cfg;
	struct pid_config *pid_cfg = &cfg->pid;
	struct psd_observer *psd_obs = &obs->psd;
	int i;
	int ret = -ENODEV;

	if (!name || len < 1)
		return ret;

	name[0] = '\0';

	if (!test_flag(MSC_FLAG_PSD_VALID, &obs->flag))
		return ret;

	for (i = 0; i < pid_cfg->num_name; i++) {
		if (!memcmp(pid_cfg->name_list[i].id, psd_obs->code.id, sizeof(pid_cfg->name_list[i].id)))
			break;
	}

	if (i < pid_cfg->num_name) {
		dev_info2(dev, "mxt get pid name %s\n", pid_cfg->name_list[i].name);
		scnprintf(name, len, ".%s", pid_cfg->name_list[i].name);
		ret = 0;
	}

	return ret;
}

static void plugin_misc_pre_process_messages(struct plugin_misc *p, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct msc_observer *obs = p->obs;

	if (test_flag(MSC_FLAG_WORKAROUND_HALT, &obs->flag))
		return;

	dev_dbg2(dev, "mxt plugin_misc_pre_process_messages pl_flag=0x%lx flag=0x%lx\n",
		 pl_flag, obs->flag);
}

static long plugin_misc_post_process_messages(struct plugin_misc *p, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct msc_observer *obs = p->obs;
	struct psd_observer *psd_obs = &obs->psd;
	struct ptc_observer *ptc_obs = &obs->ptc;

	int ret;

	if (test_flag(MSC_FLAG_WORKAROUND_HALT, &obs->flag))
		return 0;

	if (test_flag(MSC_FLAG_RESETING | MSC_FLAG_CALING, &obs->flag))
		return 0;

#if (DBG_LEVEL > 1)
	dev_info2(dev, "mxt msc pl_flag=0x%lx flag=0x%lx\n",
		 pl_flag, obs->flag);
#endif
	if (test_flag(MSC_FLAG_RESET, &obs->flag)) {
		if (test_flag(MSC_FLAG_PTC_INITED, &obs->flag)) {
			ret = -EEXIST;
			if (test_flag(MSC_FLAG_PTC_TUNED | MSC_FLAG_PTC_FAILED | MSC_FLAG_PTC_TUNING | MSC_FLAG_PTC_PARA_VALID, &obs->flag))
				ret = msc_ptc_set_param(p, obs->ptc.status.para[P_PARAM], PTC_KEY_GROUPS);
			if (ret == 0)
				set_and_clr_flag(MSC_STEP_PTC_SET_PARA, MSC_STEP_PTC_MASK, &ptc_obs->status.step);
		}
	} else if (test_flag(MSC_FLAG_CAL, &obs->flag)) {
		if (test_flag(MSC_FLAG_PTC_INITED, &obs->flag)) {
			if (test_flag(MSC_STEP_PTC_SET_PARA, &ptc_obs->status.step))
				set_and_clr_flag(MSC_STEP_PTC_IDLE, MSC_STEP_PTC_MASK, &ptc_obs->status.step);
		}
	}

	if (test_flag(MSC_FLAG_PTC_INITED, &obs->flag)) {
		if (test_flag(MSC_FLAG_PTC_TUNING, &obs->flag)) {
			if (test_flag(MSC_STEP_PTC_IDLE, &ptc_obs->status.step)) {
				ret = msc_ptc_tune_process(p);

				dev_info2(dev, "mxt msc_ptc_tune_process ret %d\n", ret);
				if (ret == 0)
					set_and_clr_flag(MSC_FLAG_PTC_TUNED, MSC_FLAG_PTC_TUNING, &obs->flag);
				else if (ret == -EAGAIN)
					set_and_clr_flag(MSC_STEP_PTC_SET_PARA, MSC_STEP_PTC_MASK, &ptc_obs->status.step);
				else if (ret == -ERANGE) {
					dev_info2(dev, "mxt msc PTC tune out of range\n");
					set_and_clr_flag(MSC_FLAG_PTC_FAILED, MSC_FLAG_PTC_TUNING, &obs->flag);
				} else {
					dev_info2(dev, "mxt msc PTC tune failed %d\n", ret);
					clear_flag(MSC_FLAG_MASK, &obs->flag);
				}
			}

			if (test_flag(MSC_FLAG_PTC_TUNED | MSC_FLAG_PTC_FAILED , &obs->flag)) {
				memcpy(obs->ptc.status.para[P_PARAM], obs->ptc.status.para[P_PARAM_B], sizeof(psd_obs->code.params));
				ret = msc_ptc_set_param(p, obs->ptc.status.para[P_PARAM], PTC_KEY_GROUPS);
				if (ret == 0) {
					set_and_clr_flag(MSC_STEP_PTC_SET_PARA, MSC_STEP_PTC_MASK, &ptc_obs->status.step);
					memcpy(psd_obs->code.params, obs->ptc.status.para[P_PARAM], sizeof(psd_obs->code.params));
					if (test_flag(MSC_FLAG_PSD_UPDATING, &obs->flag) &&
							test_flag(MSC_FLAG_PSD_VALID, &obs->flag)) {
						ret = msc_set_spare_data(p, psd_obs, psd_obs->code.id,
							sizeof(psd_obs->code.id) + sizeof(psd_obs->code.params), SET_PSD_DATA_START);
						if (ret && ret != -EAGAIN) {
							dev_err(dev, "msc_set_spare_data failed %d\n", ret);
							clear_flag(MSC_FLAG_MASK_PSD, &obs->flag);
						}
					}
				} else {
					dev_err(dev, "msc_ptc_set_param failed %d\n", ret);
					clear_flag(MSC_FLAG_MASK_PTC, &obs->flag);
				}
			}
		}
	}

	clear_flag(MSC_FLAG_MASK_LOW, &obs->flag);

	return MAX_SCHEDULE_TIMEOUT;
}

ssize_t plugin_misc_show(struct plugin_misc *p, char *buf, size_t count)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct msc_observer *obs = p->obs;
	int offset = 0;

	if (!p->init)
		return 0;

	dev_info2(dev, "[mxt]misc status: Flag=0x%08lx\n",
		obs->flag);

	offset += misc_ptc_show(p, buf + offset, count - offset);
	offset += misc_psd_show(p, buf + offset, count - offset);

	return offset;
}

int plugin_misc_store(struct plugin_misc *p, const char *buf, size_t count)
{
	printk(KERN_ERR "[mxt]plugin_misc_store: ------------------------- \n");

	if (!p->init)
		return 0;

	return count;
}

static int plugin_misc_check_tune_status(struct plugin_misc *p)
{
	struct msc_observer *obs = p->obs;


	if (!p->init)
		return -EIO;

	if (test_flag(MSC_FLAG_PTC_TUNED | MSC_FLAG_PTC_FAILED, &obs->flag))
		return 0;
	else if (test_flag(MSC_FLAG_PTC_TUNING, &obs->flag))
		return -EBUSY;
	else
		return -EINVAL;
}
static int plugin_msc_debug_show(struct plugin_misc *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct msc_observer *obs = p->obs;
	char name[256];
	int ret;

	dev_info(dev, "[mxt]PLUG_MISC_VERSION: 0x%x\n", PLUG_MISC_VERSION);

	if (!p->init)
		return 0;

	dev_info(dev, "[mxt]misc status: Flag=0x%08lx\n",
		obs->flag);

	misc_psd_show(p, NULL, 0);
	misc_ptc_show(p, NULL, 0);

	ret = plugin_misc_get_pid_name(p, name, strlen(name));
	if (ret == 0)
		dev_info(dev, "[mxt] PID name %s\n", name);
	dev_info(dev, "[mxt]\n");

	return 0;
}

static int plugin_msc_debug_store(struct plugin_misc *p, const char *buf, size_t count)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct msc_config *cfg = p->cfg;
	struct msc_observer *obs = p->obs;
	struct ptc_config *ptc_cfg = &cfg->ptc;
	struct ptc_observer *ptc_obs = &obs->ptc;
	struct psd_observer *psd_obs = &obs->psd;
	int offset, ofs, ret, val;
	char name[255];
	struct psd_code code;
	struct ptc_limit *limit;
	int k, config[1];
	s16 *para;

	dev_info(dev, "[mxt]msc store:%s\n", buf);

	if (!p->init)
		return 0;

	if (count <= 0)
		return 0;

	if (sscanf(buf, "status: Flag=0x%lx\n",
		&obs->flag) > 0) {
		dev_info(dev, "[mxt] OK\n");
	} else {
		if (count > 4 && count < sizeof(name)) {
			ret = sscanf(buf, "%s %n", name, &offset);
			dev_info2(dev, "name %s, offset %d, ret %d\n", name, offset, ret);
			if (ret == 1) {
				if (strncmp(name, "psd", 3) == 0) {
					ret = sscanf(buf + offset, "%s %n", name, &ofs);
					if (ret == 1) {
						offset += ofs;
						if (strncmp(name, "get", 3) == 0) {
							ret = msc_psd_get_data(p, &code);
							if (ret == 0) {
								misc_psd_show(p, NULL, 0);
								ret = sscanf(buf + offset, "%d", &val);
								if (ret == 1 && val == 1) {
									memcpy(&obs->psd.code, &code, sizeof(code));
									if (valid_config(obs->psd.code.id, sizeof(obs->psd.code.id), 0))
										set_flag(MSC_FLAG_PSD_VALID, &obs->flag);
								}
							}
						} else if (strncmp(name, "set", 3) == 0) {
								ret = sscanf(buf + offset, "(%d): %n", &val, &ofs);
								if (val > 0 && val < sizeof(obs->psd.code.id)) {
									for (k = 0, ofs = 0; val; k++) {
										offset += ofs;
										if (offset < count) {
											dev_info2(dev, "%s\n", buf + offset);
											ret = sscanf(buf + offset, "%x %n",
												&val, &ofs);
											if (ret == 1) {
												obs->psd.code.id[k] = (u8)val;
												dev_info2(dev, "%x", obs->psd.code.id[k]);
											} else
												break;
										} else
											break;
									}
									if (k > 0) {
										if (valid_config(obs->psd.code.id, sizeof(obs->psd.code.id), 0))
											set_flag(MSC_FLAG_PSD_VALID, &obs->flag);
									}
									misc_psd_show(p, NULL, 0);
								}
						} else if (strncmp(name, "flag", 4) == 0) {
							ret = sscanf(buf + offset, "%d", &config[0]);
							if (ret == 1) {
								psd_obs->flag = config[0];
							}
						}
					}
				} else if (strncmp(name, "ptc", 3) == 0) {
					ret = sscanf(buf + offset, "%s %n", name, &ofs);
					if (ret == 1) {
						offset += ofs;
						if (strncmp(name, "set", 6) == 0) {
							ret = sscanf(buf + offset, "%d", &config[0]);
							if (ret == 1) {
								config[0] = ((config[0]) << MSC_FLAG_PTC_MASK_SHIFT) & MSC_FLAG_MASK_PTC;
								set_and_clr_flag(config[0], MSC_FLAG_MASK_PTC, &obs->flag);
								if (test_and_clear_flag(MSC_FLAG_PTC_FAILED, &obs->flag)) {
									msc_ptc_reset_param(p, &obs->ptc.status, PTC_STATUS_REINIT);
									msc_ptc_set_param(p, obs->ptc.status.para[P_PARAM], PTC_KEY_GROUPS);
								}
							}
							dev_info(dev, "[mxt]OK(Flag = 0x%08lx)\n", obs->flag);
						} else if (strncmp(name, "tune", 6) == 0) {
							val = -1;
							ret = sscanf(buf + offset, "%d", &val);

							if (test_flag(MSC_FLAG_PTC_INITED, &obs->flag)) {

								if (val == 0) {
									dev_info(dev, "[mxt] Tune once\n");
									if (!test_flag(MSC_FLAG_PTC_TUNED | MSC_FLAG_PTC_TUNING | MSC_FLAG_PTC_PARA_VALID, &obs->flag)) {
										set_and_clr_flag(MSC_FLAG_PTC_TUNING, MSC_FLAG_MASK_PTC, &obs->flag);
									}
								} else if (val == 1) {
									dev_info(dev, "[mxt] Tune once store\n");
									if (!test_flag(MSC_FLAG_PTC_TUNED | MSC_FLAG_PTC_TUNING | MSC_FLAG_PTC_PARA_VALID, &obs->flag)) {
										set_and_clr_flag(MSC_FLAG_PTC_TUNING, MSC_FLAG_MASK_PTC, &obs->flag);
										if (test_flag(MSC_FLAG_PSD_VALID, &obs->flag))
											set_and_clr_flag(MSC_FLAG_PSD_UPDATING, MSC_FLAG_MASK_PSD, &obs->flag);
									}
								} else if (val == 2) {
									dev_info(dev, "[mxt] Tune again\n");
									set_and_clr_flag(MSC_FLAG_PTC_TUNING, MSC_FLAG_MASK_PTC, &obs->flag);
									clear_flag(MSC_FLAG_MASK_PSD, &obs->flag);
									msc_ptc_reset_param(p, &obs->ptc.status, PTC_STATUS_CONTINUE);
								} else if (val == 3) {
									dev_info(dev, "[mxt] Tune again and store\n");
									set_and_clr_flag(MSC_FLAG_PTC_TUNING, MSC_FLAG_MASK_PTC, &obs->flag);
									if (test_flag(MSC_FLAG_PSD_VALID, &obs->flag))
										set_and_clr_flag(MSC_FLAG_PSD_UPDATING, MSC_FLAG_MASK_PSD, &obs->flag);
									msc_ptc_reset_param(p, &obs->ptc.status, PTC_STATUS_CONTINUE);
								} else
									msc_ptc_get_data(p, obs->ptc.status.para[P_REF], PTC_KEY_GROUPS);
							} else {
								dev_info(dev, "[mxt] PTC not initialized\n");
							}
							dev_info(dev, "[mxt]OK(Flag = 0x%08lx)\n", obs->flag);
						} else if (strncmp(name, "range:", 5) == 0) {
							limit = &ptc_cfg->limit;
							for (k = 0, ofs = 0; k < limit->num_param; k++) {
								offset += ofs;
								if (offset < count) {
									dev_info2(dev, "%s\n", buf + offset);
									ret = sscanf(buf + offset, "%x %n",
										&val, &ofs);
									if (ret == 1) {
										limit->param[k] = (s16)val;
										dev_info2(dev, "%x", limit->param[k]);
									} else
										break;
								} else
									break;
							}
							if (k && ret > 0) {
								print_dec16_buf(KERN_INFO, "PTC range:", &limit->param[0], limit->num_param);
								dev_info2(dev, "set buf data %d\n", k);
							}
						} else if (strncmp(name, "para", 4) == 0) {
							ret = sscanf(buf + offset, "%d:%n", &config[0], &ofs);
							offset += ofs;
							if (ret == 1 && config[0] < NUM_PTC_STATUS) {
								para = ptc_obs->status.para[config[0]];
								for (k = 0, ofs = 0; k < PTC_KEY_GROUPS; k++) {
									offset += ofs;
									if (offset < count) {
										dev_info2(dev, "%s\n", buf + offset);
										ret = sscanf(buf + offset, "%x %n",
											&val, &ofs);
										if (ret == 1) {
											para[k] = (s16)val;
											dev_info2(dev, "%x", para[k]);
										} else
											break;
									} else
										break;
								}
								if (k && ret > 0) {
									print_dec16_buf(KERN_INFO, "PTC para:", para, PTC_KEY_GROUPS);
									dev_info2(dev, "set buf data %d\n", k);
								}
							}
						} else if (strncmp(name, "flag", 4) == 0) {
							ret = sscanf(buf + offset, "%d", &config[0]);
							if (ret == 1) {
								ptc_obs->status.flag = config[0];
							}
						} else if (strncmp(name, "step", 4) == 0) {
							ret = sscanf(buf + offset, "%d", &config[0]);
							if (ret == 1) {
								ptc_obs->status.step = config[0];
							}
						}
						if (p->active_thread)
								p->active_thread(p->dev);
					} else {
						dev_err(dev, "Unknow command: %s\n", buf + offset);
						return -EINVAL;
					}
				} else if (strncmp(name, "store", 5) == 0) {
					set_and_clr_flag(MSC_FLAG_PSD_UPDATING, MSC_FLAG_MASK_PSD, &obs->flag);
					ret = msc_set_spare_data(p, psd_obs, psd_obs->code.id,
						sizeof(psd_obs->code.id) + sizeof(psd_obs->code.params), SET_PSD_DATA_START);
					if (ret) {
						clear_flag(MSC_FLAG_MASK_PSD, &obs->flag);
						dev_info(dev, "set spare failed %d\n", ret);
					}
				} else {
					dev_err(dev, "Unknow msc command: %s\n", buf);
					return -EINVAL;
				}
			} else {
				dev_err(dev, "Unknow parameter, ret %d\n", ret);
			}
		}
	}

	return 0;
}

static int init_psd(struct plugin_misc *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct msc_config *cfg = p->cfg;
	struct msc_observer *obs = p->obs;
	struct psd_observer *psd_obs = &obs->psd;
	struct psd_config *psd_cfg = &cfg->psd;
	int ret;

	struct reg_config rcfg = {.reg = MXT_SPARE_T68, 0,
			.offset = 0, .buf = {0}, .len = 0, .mask = 0, .flag = 0};

	dev_info(dev, "init_psd\n");

	ret = p->get_obj_cfg(p->dev, &rcfg, 0);
	if (ret != 0) {
		dev_err(dev, "Failed get T68 information %d\n", ret);
		return ret;
	}
	psd_cfg->reg_len = rcfg.reg_len;
	psd_cfg->data_len = rcfg.reg_len - sizeof(struct t68_config_head) - sizeof(struct t68_config_tail);

	psd_obs->frame_buf = kzalloc(psd_cfg->reg_len, GFP_KERNEL);
	if (!psd_obs->frame_buf) {
		dev_err(dev, "Failed alloc T68 data buffer %d\n", psd_cfg->reg_len);
		return -ENOMEM;
	}

	psd_obs->head = (struct t68_config_head *)psd_obs->frame_buf;
	psd_obs->tail = (struct t68_config_tail *)(psd_obs->frame_buf + psd_cfg->reg_len - sizeof(struct t68_config_tail));

	ret = msc_psd_get_data(p, &psd_obs->code);
	if (ret == 0) {
		if (valid_config(psd_obs->code.id, sizeof(psd_obs->code.id), 0))
			set_flag(MSC_FLAG_PSD_VALID, &obs->flag);
	}
	return 0;
}

static void deinit_psd(struct plugin_misc *p)
{
	struct msc_observer *obs = p->obs;
	struct psd_observer *psd_obs;

	if (obs) {
		psd_obs = &obs->psd;
		if (psd_obs->frame_buf) {
			kfree(psd_obs->frame_buf);
			psd_obs->frame_buf = NULL;
		}
	}
}

static struct pid_name pid_name_map_list[] = {
    {"01", {0x34, 0x35, 0x32, 0x31, 0x00, 0x00, 0x00, 0x00} },
    {"02", {0x34, 0x35, 0x32, 0x31, 0x00, 0x00, 0x00, 0x01} },
    {"03", {0x34, 0x35, 0x32, 0x31, 0x00, 0x00, 0x00, 0x02} },
    {"04", {0x34, 0x35, 0x32, 0x31, 0x00, 0x00, 0x00, 0x03} },
};

static int init_pid(struct plugin_misc *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct msc_config *cfg = p->cfg;
	struct pid_config *pid_cfg = &cfg->pid;

	dev_dbg(dev, "init_pid\n");

	pid_cfg->name_list = &pid_name_map_list[0];
	pid_cfg->num_name = ARRAY_SIZE(pid_name_map_list);

	return 0;
}

static void deinit_pid(struct plugin_misc *p)
{

}

static s16 ptc_param_limit[NUM_LIMIT] = {
	1,
	7000,
	200,
	PTC_TUNING_MAX_COUNT,
	PTC_TARGE_REF_VAL - 50,
	PTC_TARGE_REF_VAL + 50,
};

static int init_ptc(struct plugin_misc *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct msc_config *cfg = p->cfg;
	struct msc_observer *obs = p->obs;
	struct ptc_config *ptc_cfg = &cfg->ptc;
	struct ptc_observer *ptc_obs = &obs->ptc;
	struct ptc_status *sts = &ptc_obs->status;
	struct ptc_status s1;
	int i;
	int ret;

	dev_dbg(dev, "init_ptc\n");

	memcpy(&ptc_cfg->init.para[P_PARAM], &dcfg->t96.params, sizeof(ptc_cfg->init.para[P_PARAM]));
	if (!valid_config(ptc_cfg->init.para[P_PARAM], sizeof(ptc_cfg->init.para[P_PARAM]), 0)) {
		dev_err(dev, "Invalid PTC config\n");
		return 0;
	}

	for (i = 0; i <  PTC_KEY_GROUPS; i++)
		ptc_cfg->init.para[P_STEP][i] = PTC_TUNING_STEP_VAL;
	memcpy(&ptc_obs->status, &ptc_cfg->init, sizeof(ptc_obs->status));

	ptc_cfg->target = PTC_TARGE_REF_VAL;
	ptc_cfg->limit.param = ptc_param_limit;
	ptc_cfg->limit.num_param = ARRAY_SIZE(ptc_param_limit);

	if (test_flag(MSC_FLAG_PSD_VALID, &obs->flag)) {
		memcpy(&s1, &ptc_obs->status, sizeof(s1));
		memcpy(s1.para[P_PARAM], obs->psd.code.params, sizeof(s1.para[P_PARAM]));
		ret = ptc_params_valid(ptc_cfg, &s1, P_PARAM, LIMIT_PARAM_MIN, LIMIT_PARAM_MAX);
		if (ret == 0) {
			memcpy(&ptc_obs->status, &s1, sizeof(s1));
			set_flag(MSC_FLAG_PTC_PARA_VALID, &obs->flag);

			dev_info2(dev, "Get valid PTC parameter\n");
			misc_ptc_show(p, NULL, 0);
		} else
			dev_err(dev, "No valid PTC parameters\n");
	}

	set_flag(MSC_FLAG_PTC_INITED, &obs->flag);
	set_and_clr_flag(MSC_STEP_PTC_IDLE, MSC_STEP_PTC_MASK, &sts->step);

	return 0;
}

static void deinit_ptc(struct plugin_misc *p)
{

}

static int init_msc_object(struct plugin_misc *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	int ret;

	ret = init_psd(p);
	if (ret) {
		dev_err(dev, "Failed to init_psd %s\n", __func__);
		return -ENOMEM;
	}

	ret = init_pid(p);
	if (ret) {
		dev_err(dev, "Failed to init_pid %s\n", __func__);
		return -ENOMEM;
	}

	ret = init_ptc(p);
	if (ret) {
		dev_err(dev, "Failed to init_ptc %s\n", __func__);
		return -ENOMEM;
	}

	return ret;
}

static int deinit_msc_object(struct plugin_misc *p)
{
	deinit_ptc(p);
	deinit_pid(p);
	deinit_psd(p);

	return 0;
}

static int plugin_misc_msc_init(struct plugin_misc *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;

	dev_info(dev, "%s: plugin misc msc version 0x%x\n",
			__func__, PLUG_MISC_VERSION);

	p->obs = kzalloc(sizeof(struct msc_observer), GFP_KERNEL);
	if (!p->obs) {
		dev_err(dev, "Failed to allocate memory for msc observer\n");
		return -ENOMEM;
	}

	p->cfg = kzalloc(sizeof(struct msc_config), GFP_KERNEL);
	if (!p->cfg) {
		dev_err(dev, "Failed to allocate memory for msc cfg\n");
		kfree(p->obs);
		p->obs = NULL;
		return -ENOMEM;
	}

	return init_msc_object(p);
}

static void plugin_misc_msc_deinit(struct plugin_misc *p)
{
	deinit_msc_object(p);

	if (p->obs) {
		kfree(p->obs);
		p->obs = NULL;
	}
	if (p->cfg) {
		kfree(p->cfg);
		p->cfg = NULL;
	}
}

struct plugin_misc mxt_plugin_misc = {
	.init = plugin_misc_msc_init,
	.deinit = plugin_misc_msc_deinit,
	.start = NULL,
	.stop = NULL,
	.hook_t6 = plugin_misc_hook_t6,
	.hook_t68 = plugin_misc_hook_t68,
	.get_pid_name = plugin_misc_get_pid_name,
	.pre_process = plugin_misc_pre_process_messages,
	.post_process = plugin_misc_post_process_messages,
	.show = plugin_msc_debug_show,
	.store = plugin_msc_debug_store,
	.check_tune_status = plugin_misc_check_tune_status,
};

int plugin_misc_init(struct plugin_misc *p)
{
	memcpy(p, &mxt_plugin_misc, sizeof(struct plugin_misc));

	return 0;
}

