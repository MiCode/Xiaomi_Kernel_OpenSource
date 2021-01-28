// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#include <linux/netlink.h>	/* netlink */
#ifdef CONFIG_OF
#include <linux/of_fdt.h>	/*of_dt API*/
#endif
#include <linux/platform_device.h>
#include <linux/reboot.h>	/*kernel_power_off*/
#include <linux/skbuff.h>	/* netlink */
#include <linux/socket.h>	/* netlink */
#include <net/sock.h>		/* netlink */
#include "mtk_battery_daemon.h"
#include "mtk_battery.h"

static struct sock *mtk_battery_sk;
static u_int g_fgd_pid;

static int interpolation(int i1, int b1, int i2, int b2, int i)
{
	int ret;

	ret = (b2 - b1) * (i - i1) / (i2 - i1) + b1;
	return ret;
}

/* ============================================================ */
/* Customized function */
/* ============================================================ */
int get_customized_aging_factor(int orig_af)
{
	return (orig_af + 100);
}

int get_customized_d0_c_soc(int origin_d0_c_soc)
{
	int val;

	val = (origin_d0_c_soc + 0);
	return val;
}

int get_customized_d0_v_soc(int origin_d0_v_soc)
{
	int val;

	val = (origin_d0_v_soc + 0);
	return val;
}

int get_customized_uisoc(int origin_uisoc)
{
	int val;

	val = (origin_uisoc + 0);

	return val;
}

int fg_get_system_sec(void)
{
	struct timespec time;

	time.tv_sec = 0;
	time.tv_nsec = 0;
	get_monotonic_boottime(&time);
	return (int)time.tv_sec;
}

int fg_get_imix(void)
{
	/* todo: in ALPS */
	return 1;
}

int gauge_set_nag_en(struct mtk_battery *gm, int nafg_zcv_en)
{
	if (gm->disableGM30)
		return 0;

	if (gm->disable_nafg_int == false)
		gauge_set_property(GAUGE_PROP_NAFG_EN, nafg_zcv_en);

	bm_debug(
		"%s = %d\n",
		__func__,
		nafg_zcv_en);

	return 0;
}

int gauge_get_average_current(struct mtk_battery *gm, bool *valid)
{
	int iavg = 0;
	int ver;

	ver = gauge_get_int_property(GAUGE_PROP_HW_VERSION);
	if (gm->disableGM30)
		iavg = 0;
	else {
		if (ver >= GAUGE_HW_V1000 &&
			ver < GAUGE_HW_V2000)
			iavg = gm->sw_iavg;
		else
			*valid = gm->gauge->fg_hw_info.current_avg_valid;
	}

	return iavg;
}

void mtk_battery_send_to_user(int seq, struct fgd_nl_msg_t *reply_msg)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	u32 pid = g_fgd_pid;

	int size = reply_msg->fgd_data_len + FGD_NL_HDR_LEN;
	int len = NLMSG_SPACE(size);
	void *data;
	int ret;

	reply_msg->identity = FGD_NL_MAGIC;

	if (in_interrupt())
		skb = alloc_skb(len, GFP_ATOMIC);
	else
		skb = alloc_skb(len, GFP_KERNEL);

	if (!skb)
		return;

	nlh = nlmsg_put(skb, pid, seq, 0, size, 0);
	data = NLMSG_DATA(nlh);
	memcpy(data, reply_msg, size);
	NETLINK_CB(skb).portid = 0;	/* from kernel */
	NETLINK_CB(skb).dst_group = 0;	/* unicast */

	ret = netlink_unicast(mtk_battery_sk, skb, pid, MSG_DONTWAIT);
	if (ret < 0) {
		bm_err("[%s]send failed ret=%d pid=%d\n", __func__, ret, pid);
		return;
	}
}

void fg_cmd_check(struct fgd_nl_msg_t *msg)
{
	while (msg->fgd_subcmd == 0 &&
		msg->fgd_subcmd_para1 != FGD_NL_HDR_LEN) {
		bm_err("fuel gauge version error cmd:%d %d\n",
			msg->fgd_cmd,
			msg->fgd_subcmd);
		msleep(10000);
		break;
	}
}

void fg_daemon_send_data(struct mtk_battery *gm,
	int cmd, char *rcv, char *ret)
{
	struct fgd_cmd_param_t_4 *prcv;
	struct fgd_cmd_param_t_4 *pret;

	prcv = (struct fgd_cmd_param_t_4 *)rcv;
	pret = (struct fgd_cmd_param_t_4 *)ret;

	bm_debug("%s type:%d, tsize:%d size:%d idx:%d\n",
		__func__,
		cmd,
		prcv->total_size,
		prcv->size,
		prcv->idx);

		pret->total_size = prcv->total_size;
		pret->size = prcv->size;
		pret->idx = prcv->idx;


	switch (cmd) {
	case FG_DAEMON_CMD_SEND_CUSTOM_TABLE:
		{
			char *ptr;

			if (sizeof(struct fgd_cmd_param_t_custom)
				!= prcv->total_size) {
				bm_err("%s size is different %d %d\n",
				__func__,
				(int)sizeof(
				struct fgd_cmd_param_t_custom),
				prcv->total_size);
			}

			ptr = (char *)&gm->fg_data;
			memcpy(&ptr[prcv->idx],
				prcv->input,
				prcv->size);

			bm_debug(
				"FG_DATA_TYPE_TABLE type:%d size:%d %d idx:%d\n",
				FG_DAEMON_CMD_SEND_CUSTOM_TABLE,
				prcv->total_size,
				prcv->size,
				prcv->idx);
		}
		break;

	default:
		bm_err("%s bad cmd 0x%x\n",
			__func__, cmd);
		break;
	}
}

void fg_daemon_get_data(int cmd,
	char *rcv,
	char *ret)
{
	struct fgd_cmd_param_t_4 *prcv;
	struct fgd_cmd_param_t_4 *pret;
	struct mtk_battery *gm;

	gm = get_mtk_battery();
	prcv = (struct fgd_cmd_param_t_4 *)rcv;
	pret = (struct fgd_cmd_param_t_4 *)ret;

	bm_debug("%s type:%d, tsize:%d size:%d idx:%d\n",
		__func__,
		cmd,
		prcv->total_size,
		prcv->size,
		prcv->idx);

		pret->total_size = prcv->total_size;
		pret->size = prcv->size;
		pret->idx = prcv->idx;


	switch (cmd) {
	case FG_DAEMON_CMD_GET_CUSTOM_SETTING:
	{
		char *ptr;

		if (sizeof(struct fuel_gauge_custom_data)
			!= prcv->total_size) {
			bm_err("%s size is different %d %d\n",
			__func__,
			(int)sizeof(
			struct fuel_gauge_custom_data),
			prcv->total_size);
		}

		ptr = (char *)&gm->fg_cust_data;
		memcpy(pret->input, &ptr[prcv->idx], pret->size);
		bm_trace(
			"FG_DATA_TYPE_TABLE type:%d size:%d %d idx:%d data:%d %d %d %d\n",
			FG_DAEMON_CMD_GET_CUSTOM_SETTING,
			pret->total_size,
			pret->size,
			pret->idx,
			pret->input[0],
			pret->input[1],
			pret->input[2],
			pret->input[3]);

	}
	break;
	case FG_DAEMON_CMD_GET_CUSTOM_TABLE:
		{
			char *ptr;

			if (sizeof(struct fuel_gauge_table_custom_data)
				!= prcv->total_size) {
				bm_err("%s size is different %d %d\n",
				__func__,
				(int)sizeof(
				struct fuel_gauge_table_custom_data),
				prcv->total_size);
			}

			ptr = (char *)&gm->fg_table_cust_data;
			memcpy(pret->input, &ptr[prcv->idx],
				pret->size);
			bm_trace(
				"FG_DATA_TYPE_TABLE type:%d size:%d %d idx:%d\n",
				FG_DAEMON_CMD_GET_CUSTOM_TABLE,
				prcv->total_size,
				prcv->size,
				prcv->idx);
		}
		break;
	default:
		bm_err("%s bad cmd:0x%x\n",
			__func__, cmd);
		break;

	}

}

static void mtk_battery_daemon_handler(void *nl_data,
	struct fgd_nl_msg_t *ret_msg)
{
	struct fgd_nl_msg_t *msg;
	static int ptim_vbat, ptim_i;
	struct mtk_battery *gm;
	int int_value;
	static int badcmd;

	gm = get_mtk_battery();

	msg = nl_data;
	ret_msg->nl_cmd = msg->nl_cmd;
	ret_msg->fgd_cmd = msg->fgd_cmd;

	switch (msg->fgd_cmd) {
	case FG_DAEMON_CMD_IS_BAT_EXIST:
	{
		int is_bat_exist = 0;

		gauge_get_property(GAUGE_PROP_BATTERY_EXIST, &is_bat_exist);
		ret_msg->fgd_data_len += sizeof(is_bat_exist);
		memcpy(ret_msg->fgd_data,
			&is_bat_exist, sizeof(is_bat_exist));

		bm_debug(
			"[K]FG_DAEMON_CMD_IS_BAT_EXIST=%d\n",
			is_bat_exist);
	}
	break;
	case FG_DAEMON_CMD_FGADC_RESET:
	{
		bm_debug("[K]FG_DAEMON_CMD_FGADC_RESET\n");
		battery_set_property(BAT_PROP_FG_RESET, 0);
	}
	break;
	case FG_DAEMON_CMD_GET_INIT_FLAG:
	{
		ret_msg->fgd_data_len += sizeof(gm->init_flag);
		memcpy(ret_msg->fgd_data,
			&gm->init_flag, sizeof(gm->init_flag));
		bm_debug(
			"[K]FG_DAEMON_CMD_GET_INIT_FLAG=%d\n",
			gm->init_flag);
	}
	break;
	case FG_DAEMON_CMD_SET_INIT_FLAG:
	{
		memcpy(&gm->init_flag,
			&msg->fgd_data[0], sizeof(gm->init_flag));

		if (gm->init_flag == 1)
			gauge_set_property(GAUGE_PROP_SHUTDOWN_CAR, -99999);

		bm_debug(
			"[K]FG_DAEMON_CMD_SET_INIT_FLAG=%d\n",
			gm->init_flag);
	}
	break;
	case FG_DAEMON_CMD_GET_TEMPERTURE:	/* fix me */
		{
			bool update;
			int temperture = 0;

			memcpy(&update, &msg->fgd_data[0], sizeof(update));
			temperture = force_get_tbat(gm, true);
			bm_debug("[K]FG_DAEMON_CMD_GET_TEMPERTURE update=%d tmp:%d\n",
				update, temperture);
			ret_msg->fgd_data_len += sizeof(temperture);
			memcpy(ret_msg->fgd_data,
				&temperture, sizeof(temperture));
			/* gFG_temp = temperture; */
		}
	break;
	case FG_DAEMON_CMD_GET_RAC:
	{
		int rac;

		rac = gauge_get_int_property(
				GAUGE_PROP_PTIM_RESIST);
		ret_msg->fgd_data_len += sizeof(rac);
		memcpy(ret_msg->fgd_data, &rac, sizeof(rac));
		bm_debug("[K]FG_DAEMON_CMD_GET_RAC=%d\n", rac);
	}
	break;
	case FG_DAEMON_CMD_GET_DISABLE_NAFG:
	{
		int ret = 0;

		if (gm->ntc_disable_nafg == true)
			ret = 1;
		else
			ret = 0;
		ret_msg->fgd_data_len += sizeof(ret);
		memcpy(ret_msg->fgd_data, &ret, sizeof(ret));
		bm_debug(
			"[K]FG_DAEMON_CMD_GET_DISABLE_NAFG=%d\n",
			ret);
	}
	break;
	case FG_DAEMON_CMD_GET_PTIM_VBAT:
	{
		unsigned int ptim_bat_vol = 0;
		signed int ptim_R_curr = 0;
		struct power_supply *psy;
		union power_supply_propval val;

		psy = gm->gauge->psy;
		if (gm->init_flag == 1) {
			ptim_bat_vol = gauge_get_int_property(
				GAUGE_PROP_PTIM_BATTERY_VOLTAGE) * 10;
			power_supply_get_property(psy,
				POWER_SUPPLY_PROP_CURRENT_NOW, &val);
			bm_debug("[K]PTIM V %d I %d\n",
				ptim_bat_vol, ptim_R_curr);
		} else {
			ptim_bat_vol = gm->ptim_lk_v;
			ptim_R_curr = gm->ptim_lk_i;
			if (ptim_bat_vol == 0) {
				ptim_bat_vol = gauge_get_int_property(
					GAUGE_PROP_PTIM_BATTERY_VOLTAGE) * 10;
				power_supply_get_property(psy,
					POWER_SUPPLY_PROP_CURRENT_NOW, &val);
			}
			bm_debug("[K]PTIM_LK V %d:%d I %d:%d\n",
				gm->ptim_lk_v, ptim_bat_vol,
				gm->ptim_lk_i, ptim_R_curr);
		}
		ptim_vbat = ptim_bat_vol;
		ptim_i = ptim_R_curr;
		ret_msg->fgd_data_len += sizeof(ptim_vbat);
		memcpy(ret_msg->fgd_data,
			&ptim_vbat, sizeof(ptim_vbat));
	}
	break;
	case FG_DAEMON_CMD_GET_PTIM_I:
	{
		ret_msg->fgd_data_len += sizeof(ptim_i);
		memcpy(ret_msg->fgd_data, &ptim_i, sizeof(ptim_i));
		bm_debug(
			"[K]FG_DAEMON_CMD_GET_PTIM_I=%d\n", ptim_i);
	}
	break;
	case FG_DAEMON_CMD_IS_CHARGER_EXIST:
	{
		int is_charger_exist = 0;

		if (gm->bs_data.bat_status == POWER_SUPPLY_STATUS_CHARGING)
			is_charger_exist = false;
		else
			is_charger_exist = true;

		ret_msg->fgd_data_len += sizeof(is_charger_exist);
		memcpy(ret_msg->fgd_data,
			&is_charger_exist, sizeof(is_charger_exist));

		bm_debug(
			"[K]FG_DAEMON_CMD_IS_CHARGER_EXIST=%d\n",
			is_charger_exist);
	}
	break;
	case FG_DAEMON_CMD_GET_HW_OCV:
	{
		int voltage = 0;

		gm->bs_data.bat_batt_temp = force_get_tbat(gm, true);
		voltage = gauge_get_int_property(
				GAUGE_PROP_BOOT_ZCV);
		gm->gauge->hw_status.hw_ocv = voltage;

		ret_msg->fgd_data_len += sizeof(voltage);
		memcpy(ret_msg->fgd_data, &voltage, sizeof(voltage));
		bm_debug("[K]FG_DAEMON_CMD_GET_HW_OCV=%d\n", voltage);

#ifdef GM_SIMULATOR
		gm->log.phone_state = 1;
		gm->log.ps_logtime = fg_get_log_sec();
		gm->log.ps_system_time = fg_get_system_sec();
		gm3_log_dump(true);
#endif
	}
	break;
	case FG_DAEMON_CMD_GET_FG_SW_CAR_NAFG_CNT:
	{
		int cnt = 0;
		int update;

		memcpy(&update, &msg->fgd_data[0], sizeof(update));

		if (update == 1)
			cnt = gauge_get_int_property(GAUGE_PROP_NAFG_CNT);
		else
			cnt = gm->gauge->hw_status.nafg_cnt;

		ret_msg->fgd_data_len += sizeof(cnt);
		memcpy(ret_msg->fgd_data, &cnt, sizeof(cnt));

		bm_debug(
			"[K]BATTERY_METER_CMD_GET_SW_CAR_NAFG_CNT=%d\n",
			cnt);
	}
	break;
	case FG_DAEMON_CMD_GET_FG_SW_CAR_NAFG_DLTV:
	{
		int dltv = 0;
		int update;

		memcpy(&update, &msg->fgd_data[0], sizeof(update));

		if (update == 1)
			dltv = gauge_get_int_property(GAUGE_PROP_NAFG_DLTV);
		else
			dltv = gm->gauge->hw_status.nafg_dltv;

		ret_msg->fgd_data_len += sizeof(dltv);
		memcpy(ret_msg->fgd_data, &dltv, sizeof(dltv));

		bm_debug(
			"[K]BATTERY_METER_CMD_GET_SW_CAR_NAFG_DLTV=%d\n",
			dltv);
	}
	break;
	case FG_DAEMON_CMD_GET_FG_SW_CAR_NAFG_C_DLTV:
	{
		int c_dltv = 0;
		int update;

		memcpy(&update, &msg->fgd_data[0], sizeof(update));

		if (update == 1)
			c_dltv = gauge_get_int_property(GAUGE_PROP_NAFG_C_DLTV);
		else
			c_dltv = gm->gauge->hw_status.nafg_c_dltv;

		ret_msg->fgd_data_len += sizeof(c_dltv);
		memcpy(ret_msg->fgd_data, &c_dltv, sizeof(c_dltv));

		bm_debug(
			"[K]BATTERY_METER_CMD_GET_SW_CAR_NAFG_C_DLTV=%d\n",
			c_dltv);
	}
	break;
	case FG_DAEMON_CMD_GET_FG_HW_CAR:
	{
		int fg_coulomb = 0;

		fg_coulomb = gauge_get_int_property(GAUGE_PROP_COULOMB);
		ret_msg->fgd_data_len += sizeof(fg_coulomb);
		memcpy(ret_msg->fgd_data,
			&fg_coulomb, sizeof(fg_coulomb));

		bm_debug(
			"[K]BATTERY_METER_CMD_GET_FG_HW_CAR=%d\n",
			fg_coulomb);
	}
	break;
	case FG_DAEMON_CMD_SET_SW_OCV:
	{
		int _sw_ocv;

		memcpy(&_sw_ocv, &msg->fgd_data[0], sizeof(_sw_ocv));
		gm->gauge->hw_status.sw_ocv = _sw_ocv;
		bm_debug("[K]FG_DAEMON_CMD_SET_SW_OCV=%d\n", _sw_ocv);
	}
	break;
	case FG_DAEMON_CMD_SET_FG_TIME:
	{
		int secs;
		struct timespec time, time_now, end_time;
		ktime_t ktime;

		memcpy(&secs, &msg->fgd_data[0], sizeof(secs));

		if (secs != 0 && secs > 0) {
			get_monotonic_boottime(&time_now);
			time.tv_sec = secs;
			time.tv_nsec = 0;

			end_time = timespec_add(time_now, time);
			ktime = ktime_set(end_time.tv_sec, end_time.tv_nsec);

			if (msg->fgd_subcmd_para1 == 0)
				alarm_start(&gm->tracking_timer, ktime);
			else
				alarm_start(&gm->one_percent_timer, ktime);
		} else {
			if (msg->fgd_subcmd_para1 == 0)
				alarm_cancel(&gm->tracking_timer);
			else
				alarm_cancel(&gm->one_percent_timer);
		}

		bm_debug("[K]FG_DAEMON_CMD_SET_FG_TIME=%d cmd:%d %d\n",
			secs,
			msg->fgd_subcmd, msg->fgd_subcmd_para1);
	}
	break;
	case FG_DAEMON_CMD_GET_FG_TIME:
	{
		int now_time_secs;

		now_time_secs = fg_get_system_sec();
		ret_msg->fgd_data_len += sizeof(now_time_secs);
		memcpy(ret_msg->fgd_data,
			&now_time_secs, sizeof(now_time_secs));
		bm_debug(
			"[K]FG_DAEMON_CMD_GET_NOW_TIME=%d\n",
			now_time_secs);
	}
	break;
	case FG_DAEMON_CMD_SET_FG_BAT_INT1_GAP:
	{
		int fg_coulomb = 0;

		fg_coulomb = gauge_get_int_property(GAUGE_PROP_COULOMB);

		memcpy(&gm->coulomb_int_gap,
			&msg->fgd_data[0], sizeof(gm->coulomb_int_gap));

		gm->coulomb_int_ht = fg_coulomb + gm->coulomb_int_gap;
		gm->coulomb_int_lt = fg_coulomb - gm->coulomb_int_gap;
		gauge_coulomb_start(&gm->coulomb_plus, gm->coulomb_int_gap);
		gauge_coulomb_start(&gm->coulomb_minus, -gm->coulomb_int_gap);

		bm_debug(
			"[K]FG_DAEMON_CMD_SET_FG_BAT_INT1_GAP=%d car:%d\n",
			gm->coulomb_int_gap, fg_coulomb);
	}
	break;
	case FG_DAEMON_CMD_SET_FG_BAT_INT2_HT_GAP:
	{
		memcpy(&gm->uisoc_int_ht_gap,
			&msg->fgd_data[0], sizeof(gm->uisoc_int_ht_gap));
		gauge_coulomb_start(&gm->uisoc_plus, gm->uisoc_int_ht_gap);
		bm_debug(
			"[K]FG_DAEMON_CMD_SET_FG_BAT_INT2_HT_GAP=%d\n",
			gm->uisoc_int_ht_gap);
	}
	break;
	case FG_DAEMON_CMD_SET_FG_BAT_INT2_LT_GAP:
	{
		memcpy(&gm->uisoc_int_lt_gap,
			&msg->fgd_data[0], sizeof(gm->uisoc_int_lt_gap));
		gauge_coulomb_start(&gm->uisoc_minus, -gm->uisoc_int_lt_gap);
		bm_debug(
			"[K]FG_DAEMON_CMD_SET_FG_BAT_INT2_LT_GAP=%d\n",
			gm->uisoc_int_lt_gap);
	}
	break;
	case FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_HT:
	{
		int ver;
		ktime_t ktime;

		ver = gauge_get_int_property(GAUGE_PROP_HW_VERSION);
		memcpy(&gm->uisoc_int_ht_en,
			&msg->fgd_data[0], sizeof(gm->uisoc_int_ht_en));
		if (gm->uisoc_int_ht_en == 0) {
			if (ver != GAUGE_NO_HW)
				gauge_coulomb_stop(&gm->uisoc_plus);
			else if (gm->soc != gm->ui_soc) {
				ktime = ktime_set(60, 0);
				alarm_start(&gm->sw_uisoc_timer, ktime);
			}
		}
		bm_debug(
			"[K]FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_HT=%d\n",
			gm->uisoc_int_ht_en);
	}
	break;
	case FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_LT:
	{
		memcpy(&gm->uisoc_int_lt_en,
			&msg->fgd_data[0], sizeof(gm->uisoc_int_lt_en));
		if (gm->uisoc_int_lt_en == 0)
			gauge_coulomb_stop(&gm->uisoc_minus);

		bm_debug(
			"[K]FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_LT=%d\n",
			gm->uisoc_int_lt_en);
	}
	break;
	case FG_DAEMON_CMD_SET_FG_BAT_TMP_C_GAP:
	{
		int tmp = force_get_tbat(gm, true);

		memcpy(&gm->bat_tmp_c_int_gap,
			&msg->fgd_data[0], sizeof(gm->bat_tmp_c_int_gap));

		gm->bat_tmp_c_ht = tmp + gm->bat_tmp_c_int_gap;
		gm->bat_tmp_c_lt = tmp - gm->bat_tmp_c_int_gap;

		bm_debug(
			"[K]FG_DAEMON_CMD_SET_FG_BAT_TMP_C_GAP=%d ht:%d lt:%d\n",
			gm->bat_tmp_c_int_gap,
			gm->bat_tmp_c_ht,
			gm->bat_tmp_c_lt);
	}
	break;

	case FG_DAEMON_CMD_SET_FG_BAT_TMP_GAP:
	{
		int tmp = force_get_tbat(gm, true);

		memcpy(
			&gm->bat_tmp_int_gap, &msg->fgd_data[0],
			sizeof(gm->bat_tmp_int_gap));

		gm->bat_tmp_ht = tmp + gm->bat_tmp_int_gap;
		gm->bat_tmp_lt = tmp - gm->bat_tmp_int_gap;

		bm_debug(
			"[K]FG_DAEMON_CMD_SET_FG_BAT_TMP_GAP=%d ht:%d lt:%d\n",
			gm->bat_tmp_int_gap,
			gm->bat_tmp_ht, gm->bat_tmp_lt);

	}
	break;
	case FG_DAEMON_CMD_IS_BAT_PLUGOUT:
	{
		int is_bat_plugout = 0;

		is_bat_plugout = gm->gauge->hw_status.is_bat_plugout;
		ret_msg->fgd_data_len += sizeof(is_bat_plugout);
		memcpy(ret_msg->fgd_data,
			&is_bat_plugout, sizeof(is_bat_plugout));

		bm_debug(
			"[K]BATTERY_METER_CMD_GET_BOOT_BATTERY_PLUG_STATUS=%d\n",
			is_bat_plugout);
	}
	break;
	case FG_DAEMON_CMD_GET_BAT_PLUG_OUT_TIME:
	{
		unsigned int time = 0;

		time = gm->gauge->hw_status.bat_plug_out_time;
		ret_msg->fgd_data_len += sizeof(time);
		memcpy(ret_msg->fgd_data, &time, sizeof(time));
		bm_debug(
			"[K]FG_DAEMON_CMD_GET_BAT_PLUG_OUT_TIME=%d\n",
			time);
	}
	break;
	case FG_DAEMON_CMD_IS_BAT_CHARGING:
	{
		int is_bat_charging = 0;
		int bat_current = 0;

		/* BAT_DISCHARGING = 0 */
		/* BAT_CHARGING = 1 */

		bat_current = gauge_get_int_property(
			GAUGE_PROP_BATTERY_CURRENT);
		if (bat_current > 0)
			is_bat_charging = 1;
		else
			is_bat_charging = 0;

		ret_msg->fgd_data_len += sizeof(is_bat_charging);
		memcpy(ret_msg->fgd_data,
			&is_bat_charging, sizeof(is_bat_charging));

		bm_debug(
			"[K]FG_DAEMON_CMD_IS_BAT_CHARGING=%d\n",
			is_bat_charging);
	}
	break;
	case FG_DAEMON_CMD_GET_CHARGER_STATUS:
	{
		int charger_status = 0;

		/* charger status need charger API */
		/* CHR_ERR = -1 */
		/* CHR_NORMAL = 0 */
		if (gm->bs_data.bat_status ==
			POWER_SUPPLY_STATUS_NOT_CHARGING)
			charger_status = -1;
		else
			charger_status = 0;

		ret_msg->fgd_data_len += sizeof(charger_status);
		memcpy(ret_msg->fgd_data,
			&charger_status, sizeof(charger_status));

		bm_debug(
			"[K]FG_DAEMON_CMD_GET_CHARGER_STATUS=%d\n",
			charger_status);
	}
	break;
	case FG_DAEMON_CMD_CHECK_FG_DAEMON_VERSION:
	{
		/* todo */
		bm_debug(
			"[K]FG_DAEMON_CMD_CHECK_FG_DAEMON_VERSION\n");
	}
	break;
	case FG_DAEMON_CMD_GET_SHUTDOWN_DURATION_TIME:
	{
		signed int time = 0;

		time = gm->pl_shutdown_time;

		ret_msg->fgd_data_len += sizeof(time);
		memcpy(ret_msg->fgd_data, &time, sizeof(time));
		bm_debug(
			"[K]FG_DAEMON_CMD_GET_SHUTDOWN_DURATION_TIME=%d\n",
			time);
	}
	break;
	case FG_DAEMON_CMD_SET_FG_RESET_RTC_STATUS:
	{
		int fg_reset_rtc;

		memcpy(&fg_reset_rtc, &msg->fgd_data[0], sizeof(fg_reset_rtc));
		gauge_set_property(GAUGE_PROP_RESET_FG_RTC, 0);
		bm_debug(
			"[K]BATTERY_METER_CMD_SET_FG_RESET_RTC_STATUS=%d\n",
			fg_reset_rtc);
	}
	break;
	case FG_DAEMON_CMD_SET_IS_FG_INITIALIZED:
	{
		int fg_reset;

		memcpy(&fg_reset, &msg->fgd_data[0], sizeof(fg_reset));
		gauge_set_property(GAUGE_PROP_GAUGE_INITIALIZED, fg_reset);
		bm_debug(
			"[K]BATTERY_METER_CMD_SET_FG_RESET_STATUS=%d\n",
			fg_reset);
	}
	break;
	case FG_DAEMON_CMD_GET_IS_FG_INITIALIZED:
	{
		int fg_reset;

		fg_reset = gauge_get_int_property(GAUGE_PROP_GAUGE_INITIALIZED);
		ret_msg->fgd_data_len += sizeof(fg_reset);
		memcpy(ret_msg->fgd_data, &fg_reset, sizeof(fg_reset));
		bm_debug(
			"[K]BATTERY_METER_CMD_GET_FG_RESET_STATUS=%d\n",
			fg_reset);
	}
	break;
	case FG_DAEMON_CMD_IS_HWOCV_UNRELIABLE:
	{
		int is_hwocv_unreliable;

		is_hwocv_unreliable =
			gm->gauge->hw_status.flag_hw_ocv_unreliable;
		ret_msg->fgd_data_len += sizeof(is_hwocv_unreliable);
		memcpy(ret_msg->fgd_data,
			&is_hwocv_unreliable, sizeof(is_hwocv_unreliable));
		bm_debug(
			"[K]FG_DAEMON_CMD_IS_HWOCV_UNRELIABLE=%d\n",
			is_hwocv_unreliable);
	}
	break;
	case FG_DAEMON_CMD_GET_FG_CURRENT_AVG:
	{
		int fg_current_iavg = gm->sw_iavg;
		bool valid;
		int ver;

		ver = gauge_get_int_property(GAUGE_PROP_HW_VERSION);

		if (ver >= GAUGE_HW_V2000)
			fg_current_iavg =
				gauge_get_average_current(gm, &valid);

		ret_msg->fgd_data_len += sizeof(fg_current_iavg);
		memcpy(ret_msg->fgd_data,
			&fg_current_iavg, sizeof(fg_current_iavg));
		bm_debug(
			"[K]FG_DAEMON_CMD_GET_FG_CURRENT_AVG=%d %d v:%d\n",
			fg_current_iavg, gm->sw_iavg, ver);
	}
	break;
	case FG_DAEMON_CMD_GET_FG_CURRENT_IAVG_VALID:
	{
		bool valid = false;
		int iavg_valid = true;
		int ver;

		ver = gauge_get_int_property(GAUGE_PROP_HW_VERSION);
		if (ver >= GAUGE_HW_V2000) {
			gauge_get_average_current(gm, &valid);
			iavg_valid = valid;
		}

		ret_msg->fgd_data_len += sizeof(iavg_valid);
		memcpy(ret_msg->fgd_data, &iavg_valid, sizeof(iavg_valid));
		bm_debug(
			"[K]FG_DAEMON_CMD_GET_FG_CURRENT_IAVG_VALID=%d\n",
			iavg_valid);
	}
	break;
	case FG_DAEMON_CMD_GET_ZCV:
	{
		int zcv = 0;

		zcv = gauge_get_int_property(GAUGE_PROP_ZCV);
		ret_msg->fgd_data_len += sizeof(zcv);
		memcpy(ret_msg->fgd_data, &zcv, sizeof(zcv));
		bm_debug("[K]FG_DAEMON_CMD_GET_ZCV=%d\n", zcv);
	}
	break;
	case FG_DAEMON_CMD_SET_NAG_ZCV_EN:
	{
		int nafg_zcv_en;

		memcpy(&nafg_zcv_en, &msg->fgd_data[0], sizeof(nafg_zcv_en));

		gauge_set_nag_en(gm, nafg_zcv_en);

		bm_debug(
			"[K]FG_DAEMON_CMD_SET_NAG_ZCV_EN=%d\n",
			nafg_zcv_en);
	}
	break;

	case FG_DAEMON_CMD_SET_NAG_ZCV:
	{
		int nafg_zcv;

		memcpy(&nafg_zcv, &msg->fgd_data[0], sizeof(nafg_zcv));

		gauge_set_property(GAUGE_PROP_NAFG_ZCV, nafg_zcv);
		gm->log.nafg_zcv = nafg_zcv;
		gm->gauge->hw_status.nafg_zcv = nafg_zcv;

		bm_debug("[K]BATTERY_METER_CMD_SET_NAG_ZCV=%d\n", nafg_zcv);
	}
	break;
	case FG_DAEMON_CMD_SET_NAG_C_DLTV:
	{
		int nafg_c_dltv;

		memcpy(&nafg_c_dltv, &msg->fgd_data[0], sizeof(nafg_c_dltv));
		gm->gauge->hw_status.nafg_c_dltv_th = nafg_c_dltv;
		gauge_set_property(GAUGE_PROP_NAFG_C_DLTV, nafg_c_dltv);
		gauge_set_nag_en(gm, 1);

		bm_debug(
			"[K]BATTERY_METER_CMD_SET_NAG_C_DLTV=%d\n",
			nafg_c_dltv);
	}
	break;
	case FG_DAEMON_CMD_SET_IAVG_INTR:
	{
		bm_err("[K]FG_DAEMON_CMD_SET_IAVG_INTR is removed\n");
	}
	break;
	case FG_DAEMON_CMD_SET_BAT_PLUGOUT_INTR:
	{
		int fg_bat_plugout_en;

		memcpy(&fg_bat_plugout_en,
			&msg->fgd_data[0], sizeof(fg_bat_plugout_en));
		gauge_set_property(GAUGE_PROP_BAT_PLUGOUT_EN,
			fg_bat_plugout_en);
		bm_debug(
			"[K]BATTERY_METER_CMD_SET_BAT_PLUGOUT_INTR_EN=%d\n",
			fg_bat_plugout_en);
	}
	break;
	case FG_DAEMON_CMD_SET_ZCV_INTR:
	{
		int fg_zcv_current;

		memcpy(&fg_zcv_current,
			&msg->fgd_data[0], sizeof(fg_zcv_current));

		gauge_set_property(GAUGE_PROP_ZCV_INTR_THRESHOLD,
			fg_zcv_current);
		gauge_set_property(GAUGE_PROP_ZCV_INTR_EN, 1);

		bm_debug(
			"[K]BATTERY_METER_CMD_SET_ZCV_INTR=%d\n",
			fg_zcv_current);
	}
	break;
	case FG_DAEMON_CMD_SET_FG_QUSE:/* tbd */
	{
		int fg_quse;

		memcpy(&fg_quse, &msg->fgd_data[0], sizeof(fg_quse));

		bm_debug("[K]FG_DAEMON_CMD_SET_FG_QUSE=%d\n", fg_quse);
	}
	break;
	case FG_DAEMON_CMD_SET_FG_DC_RATIO:/* tbd */
	{
		int fg_dc_ratio;

		memcpy(&fg_dc_ratio, &msg->fgd_data[0], sizeof(fg_dc_ratio));

		bm_debug(
			"[K]BATTERY_METER_CMD_SET_FG_DC_RATIO=%d\n",
			fg_dc_ratio);
	}
	break;
	case FG_DAEMON_CMD_SOFF_RESET:
	{
		gauge_set_property(GAUGE_PROP_SOFF_RESET, 1);
		bm_debug("[K]BATTERY_METER_CMD_SOFF_RESET\n");
	}
	break;
	case FG_DAEMON_CMD_NCAR_RESET:
	{
		gauge_set_property(GAUGE_PROP_NCAR_RESET, 1);
		bm_debug("[K]BATTERY_METER_CMD_NCAR_RESET\n");
	}
	break;
	case FG_DAEMON_CMD_SET_BATTERY_CYCLE_THRESHOLD:
	{
		memcpy(&gm->bat_cycle_thr,
			&msg->fgd_data[0], sizeof(gm->bat_cycle_thr));

		gauge_set_property(GAUGE_PROP_BAT_CYCLE_INTR_THRESHOLD,
			gm->bat_cycle_thr);

		bm_debug(
			"[K]FG_DAEMON_CMD_SET_BATTERY_CYCLE_THRESHOLD=%d\n",
			gm->bat_cycle_thr);

		fg_sw_bat_cycle_accu(gm);
	}
	break;
	case FG_DAEMON_CMD_GET_IMIX:
	{
		int imix = UNIT_TRANS_10 * fg_get_imix();

		ret_msg->fgd_data_len += sizeof(imix);
		memcpy(ret_msg->fgd_data, &imix, sizeof(imix));
		bm_debug("[K]FG_DAEMON_CMD_GET_IMIX=%d\n", imix);
	}
	break;
	case FG_DAEMON_CMD_IS_BATTERY_CYCLE_RESET:
	{
		int reset = gm->is_reset_battery_cycle;

		ret_msg->fgd_data_len += sizeof(reset);
		memcpy(ret_msg->fgd_data, &reset, sizeof(reset));
		bm_debug(
			"[K]FG_DAEMON_CMD_IS_BATTERY_CYCLE_RESET = %d\n",
			reset);
		gm->is_reset_battery_cycle = false;
	}
	break;
	case FG_DAEMON_CMD_GET_AGING_FACTOR_CUST:
	{
		int aging_factor_cust = 0;
		int origin_aging_factor;

		memcpy(&origin_aging_factor,
			&msg->fgd_data[0], sizeof(origin_aging_factor));
		aging_factor_cust =
			get_customized_aging_factor(origin_aging_factor);

		ret_msg->fgd_data_len += sizeof(aging_factor_cust);
		memcpy(ret_msg->fgd_data,
			&aging_factor_cust, sizeof(aging_factor_cust));
		bm_debug(
			"[K]FG_DAEMON_CMD_GET_AGING_FACTOR_CUST = %d\n",
			aging_factor_cust);
	}
	break;
	case FG_DAEMON_CMD_GET_D0_C_SOC_CUST:
	{
		int d0_c_cust = 0;
		int origin_d0_c;

		memcpy(&origin_d0_c, &msg->fgd_data[0], sizeof(origin_d0_c));
		d0_c_cust = get_customized_d0_c_soc(origin_d0_c);

		ret_msg->fgd_data_len += sizeof(d0_c_cust);
		memcpy(ret_msg->fgd_data, &d0_c_cust, sizeof(d0_c_cust));
		bm_debug(
			"[K]FG_DAEMON_CMD_GET_D0_C_CUST = %d\n",
			d0_c_cust);
	}
	break;

	case FG_DAEMON_CMD_GET_D0_V_SOC_CUST:
	{
		int d0_v_cust = 0;
		int origin_d0_v;

		memcpy(&origin_d0_v, &msg->fgd_data[0], sizeof(origin_d0_v));
		d0_v_cust = get_customized_d0_v_soc(origin_d0_v);

		ret_msg->fgd_data_len += sizeof(d0_v_cust);
		memcpy(ret_msg->fgd_data, &d0_v_cust, sizeof(d0_v_cust));
		bm_debug(
			"[K]FG_DAEMON_CMD_GET_D0_V_CUST = %d\n",
			d0_v_cust);
	}
	break;

	case FG_DAEMON_CMD_GET_UISOC_CUST:
	{
		int uisoc_cust = 0;
		int origin_uisoc;

		memcpy(&origin_uisoc, &msg->fgd_data[0], sizeof(origin_uisoc));
		uisoc_cust = get_customized_uisoc(origin_uisoc);

		ret_msg->fgd_data_len += sizeof(uisoc_cust);
		memcpy(ret_msg->fgd_data, &uisoc_cust, sizeof(uisoc_cust));
		bm_debug(
			"[K]FG_DAEMON_CMD_GET_UISOC_CUST = %d\n",
			uisoc_cust);
	}
	break;
	case FG_DAEMON_CMD_IS_KPOC:
	{
		int is_kpoc = is_kernel_power_off_charging();

		ret_msg->fgd_data_len += sizeof(is_kpoc);
		memcpy(ret_msg->fgd_data, &is_kpoc, sizeof(is_kpoc));
		bm_debug(
			"[K]FG_DAEMON_CMD_IS_KPOC = %d\n", is_kpoc);
	}
	break;
	case FG_DAEMON_CMD_GET_NAFG_VBAT:
	{
		int nafg_vbat;

		nafg_vbat = gauge_get_int_property(GAUGE_PROP_NAFG_VBAT);
		ret_msg->fgd_data_len += sizeof(nafg_vbat);
		memcpy(ret_msg->fgd_data, &nafg_vbat, sizeof(nafg_vbat));
		bm_debug(
			"[K]FG_DAEMON_CMD_GET_NAFG_VBAT = %d\n",
			nafg_vbat);
	}
	break;
	case FG_DAEMON_CMD_GET_HW_INFO:
	{
		gauge_set_property(GAUGE_PROP_HW_INFO, 1);
		bm_debug(
			"[K]FG_DAEMON_CMD_GET_HW_INFO\n");
	}
	break;
	case FG_DAEMON_CMD_SET_KERNEL_SOC:
	{
		int daemon_soc;
		int soc_type;

		soc_type = msg->fgd_subcmd_para1;

		memcpy(&daemon_soc, &msg->fgd_data[0], sizeof(daemon_soc));
		if (soc_type == 0)
			gm->soc = (daemon_soc + 50) / 100;

		bm_debug(
		"[K]FG_DAEMON_CMD_SET_KERNEL_SOC = %d %d, type:%d\n",
		daemon_soc, gm->soc, soc_type);

	}
	break;
	case FG_DAEMON_CMD_SET_KERNEL_UISOC:
	{
		int daemon_ui_soc;
		int old_uisoc;
		struct timespec now_time, diff;

		memcpy(&daemon_ui_soc, &msg->fgd_data[0],
			sizeof(daemon_ui_soc));

		if (daemon_ui_soc < 0) {
			bm_debug("FG_DAEMON_CMD_SET_KERNEL_UISOC error,daemon_ui_soc:%d\n",
				daemon_ui_soc);
			daemon_ui_soc = 0;
		}

		gm->fg_cust_data.ui_old_soc = daemon_ui_soc;
		old_uisoc = gm->ui_soc;

		if (gm->disableGM30 == true)
			gm->ui_soc = 50;
		else
			gm->ui_soc = (daemon_ui_soc + 50) / 100;

		/* when UISOC changes, check the diff time for smooth */
		if (old_uisoc != gm->ui_soc) {
			get_monotonic_boottime(&now_time);
			diff = timespec_sub(now_time, gm->uisoc_oldtime);

			bm_err("[K]FG_DAEMON_CMD_SET_KERNEL_UISOC = %d %d GM3:%d old:%d diff=%ld\n",
				daemon_ui_soc, gm->ui_soc,
				gm->disableGM30, old_uisoc, diff.tv_sec);
			gm->uisoc_oldtime = now_time;

			gm->bs_data.bat_capacity = gm->ui_soc;
			battery_update(gm);
		} else {
			bm_debug("[K]FG_DAEMON_CMD_SET_KERNEL_UISOC = %d %d GM3:%d\n",
				daemon_ui_soc, gm->ui_soc, gm->disableGM30);
			/* ac_update(&ac_main); */
			gm->bs_data.bat_capacity = gm->ui_soc;
			battery_update(gm);
		}
	}
	break;
	case FG_DAEMON_CMD_SET_KERNEL_INIT_VBAT:
	{
		int daemon_init_vbat;

		memcpy(&daemon_init_vbat, &msg->fgd_data[0],
			sizeof(daemon_init_vbat));
		bm_debug(
			"[K]FG_DAEMON_CMD_SET_KERNEL_INIT_VBAT = %d\n",
			daemon_init_vbat);
	}
	break;
	case FG_DAEMON_CMD_SET_FG_SHUTDOWN_COND:
	{
		int shutdown_cond;

		memcpy(&shutdown_cond, &msg->fgd_data[0],
			sizeof(shutdown_cond));
		set_shutdown_cond(gm, shutdown_cond);
		bm_debug(
			"[K]FG_DAEMON_CMD_SET_FG_SHUTDOWN_COND = %d\n",
			shutdown_cond);
	}
	break;
	case FG_DAEMON_CMD_GET_FG_SHUTDOWN_COND:
	{
		unsigned int shutdown_cond = get_shutdown_cond(gm);

		ret_msg->fgd_data_len += sizeof(shutdown_cond);
		memcpy(ret_msg->fgd_data,
			&shutdown_cond, sizeof(shutdown_cond));

		bm_debug("[K] shutdown_cond = %d\n", shutdown_cond);
	}
	break;
	case FG_DAEMON_CMD_ENABLE_FG_VBAT_L_INT:
	{
		int fg_vbat_l_en;

		memcpy(&fg_vbat_l_en, &msg->fgd_data[0], sizeof(fg_vbat_l_en));
		gauge_set_property(GAUGE_PROP_EN_LOW_VBAT_INTERRUPT,
			fg_vbat_l_en);
		bm_debug(
			"[K]FG_DAEMON_CMD_ENABLE_FG_VBAT_L_INT = %d\n",
			fg_vbat_l_en);
	}
	break;

	case FG_DAEMON_CMD_ENABLE_FG_VBAT_H_INT:
	{
		int fg_vbat_h_en;

		memcpy(&fg_vbat_h_en, &msg->fgd_data[0], sizeof(fg_vbat_h_en));
		gauge_set_property(GAUGE_PROP_EN_HIGH_VBAT_INTERRUPT,
			fg_vbat_h_en);
		bm_debug(
			"[K]FG_DAEMON_CMD_ENABLE_FG_VBAT_H_INT = %d\n",
			fg_vbat_h_en);
	}
	break;

	case FG_DAEMON_CMD_SET_FG_VBAT_L_TH:
	{
		int fg_vbat_l_thr;

		memcpy(&fg_vbat_l_thr,
			&msg->fgd_data[0], sizeof(fg_vbat_l_thr));
		gauge_set_property(GAUGE_PROP_VBAT_LT_INTR_THRESHOLD,
		fg_vbat_l_thr);
		set_shutdown_vbat_lt(gm,
			fg_vbat_l_thr, gm->fg_cust_data.vbat2_det_voltage1);
		bm_debug(
			"[K]FG_DAEMON_CMD_SET_FG_VBAT_L_TH = %d\n",
			fg_vbat_l_thr);
	}
	break;
	case FG_DAEMON_CMD_SET_FG_VBAT_H_TH:
	{
		int fg_vbat_h_thr;

		memcpy(&fg_vbat_h_thr, &msg->fgd_data[0],
			sizeof(fg_vbat_h_thr));
		gauge_set_property(GAUGE_PROP_VBAT_HT_INTR_THRESHOLD,
		fg_vbat_h_thr);

		bm_debug(
			"[K]FG_DAEMON_CMD_SET_FG_VBAT_H_TH=%d\n",
			fg_vbat_h_thr);
	}
	break;
	case FG_DAEMON_CMD_SET_CAR_TUNE_VALUE:
	{
		signed int cali_car_tune;

		memcpy(
			&cali_car_tune,
			&msg->fgd_data[0],
			sizeof(cali_car_tune));
#ifdef CALIBRATE_CAR_TUNE_VALUE_BY_META_TOOL
		bm_err("[K] cali_car_tune = %d, default = %d, Use [cali_car_tune]\n",
			cali_car_tune, gm->fg_cust_data.car_tune_value);
		gm->fg_cust_data.car_tune_value = cali_car_tune;
#else
		bm_err("[K] cali_car_tune = %d, default = %d, Use [default]\n",
			cali_car_tune, gm->fg_cust_data.car_tune_value);
#endif
	}
	break;
	case FG_DAEMON_CMD_GET_RTC_UI_SOC:
	{
		int rtc_ui_soc;

		rtc_ui_soc = gauge_get_int_property(GAUGE_PROP_RTC_UI_SOC);

		ret_msg->fgd_data_len += sizeof(rtc_ui_soc);
		memcpy(ret_msg->fgd_data, &rtc_ui_soc,
			sizeof(rtc_ui_soc));
		bm_debug("[K]FG_DAEMON_CMD_GET_RTC_UI_SOC = %d\n",
			rtc_ui_soc);
	}
	break;

	case FG_DAEMON_CMD_SET_RTC_UI_SOC:
	{
		int rtc_ui_soc;

		memcpy(&rtc_ui_soc, &msg->fgd_data[0], sizeof(rtc_ui_soc));

		if (rtc_ui_soc < 0) {
			bm_err("[K]FG_DAEMON_CMD_SET_RTC_UI_SOC error,rtc_ui_soc=%d\n",
				rtc_ui_soc);

			rtc_ui_soc = 0;
		}
		gauge_set_property(GAUGE_PROP_RTC_UI_SOC, rtc_ui_soc);
		bm_debug(
			"[K] BATTERY_METER_CMD_SET_RTC_UI_SOC=%d\n",
			rtc_ui_soc);
	}
	break;
	case FG_DAEMON_CMD_SET_CON0_SOC:
	{
		int _soc = 0;

		memcpy(&_soc, &msg->fgd_data[0], sizeof(_soc));
		gauge_set_property(GAUGE_PROP_CON0_SOC, _soc);
		bm_debug("[K]FG_DAEMON_CMD_SET_CON0_SOC = %d\n", _soc);
	}
	break;

	case FG_DAEMON_CMD_GET_CON0_SOC:
	{
		int _soc = 0;

		_soc = gauge_get_int_property(GAUGE_PROP_CON0_SOC);
		ret_msg->fgd_data_len += sizeof(_soc);
		memcpy(ret_msg->fgd_data, &_soc, sizeof(_soc));
		bm_debug("[K]FG_DAEMON_CMD_GET_CON0_SOC = %d\n", _soc);
	}
	break;
	case FG_DAEMON_CMD_GET_NVRAM_FAIL_STATUS:
	{
		int flag = 0;

		flag = gauge_get_int_property(GAUGE_PROP_IS_NVRAM_FAIL_MODE);
		ret_msg->fgd_data_len += sizeof(flag);
		memcpy(ret_msg->fgd_data, &flag, sizeof(flag));
		bm_debug(
			"[K]FG_DAEMON_CMD_GET_NVRAM_FAIL_STATUS = %d\n",
			flag);
	}
	break;
	case FG_DAEMON_CMD_SET_NVRAM_FAIL_STATUS:
	{
		int flag = 0;

		memcpy(&flag, &msg->fgd_data[0], sizeof(flag));
		gauge_set_property(GAUGE_PROP_IS_NVRAM_FAIL_MODE, flag);
		bm_debug(
			"[K]FG_DAEMON_CMD_SET_NVRAM_FAIL_STATUS = %d\n",
			flag);
	}
	break;
	case FG_DAEMON_CMD_GET_RTC_TWO_SEC_REBOOT:
	{
		int two_sec_reboot_flag;

		two_sec_reboot_flag = gm->pl_two_sec_reboot;
		ret_msg->fgd_data_len += sizeof(two_sec_reboot_flag);
		memcpy(ret_msg->fgd_data,
			&two_sec_reboot_flag,
			sizeof(two_sec_reboot_flag));
		bm_debug(
			"[K]FG_DAEMON_CMD_GET_RTC_TWO_SEC_REBOOT = %d\n",
			two_sec_reboot_flag);
	}
	break;
	case FG_DAEMON_CMD_GET_RTC_INVALID:
	{
		int rtc_invalid;

		rtc_invalid = gm->gauge->hw_status.rtc_invalid;
		ret_msg->fgd_data_len += sizeof(rtc_invalid);
		memcpy(ret_msg->fgd_data, &rtc_invalid, sizeof(rtc_invalid));
		bm_debug(
			"[K]FG_DAEMON_CMD_GET_RTC_INVALID = %d\n",
			rtc_invalid);
	}
	break;
	case FG_DAEMON_CMD_SET_DAEMON_PID:
	{
		fg_cmd_check(msg);
		/* check is daemon killed case*/
		if (g_fgd_pid == 0) {
			memcpy(&g_fgd_pid, &msg->fgd_data[0],
				sizeof(g_fgd_pid));
			bm_err("[K]FG_DAEMON_CMD_SET_DAEMON_PID = %d(first launch)\n",
				g_fgd_pid);
		} else {
			memcpy(&g_fgd_pid, &msg->fgd_data[0],
				sizeof(g_fgd_pid));
			bm_err("[K]FG_DAEMON_CMD_SET_DAEMON_PID = %d(re-launch)\n",
				g_fgd_pid);
			/* kill daemon dod_init 14 , todo*/
		}
	}
	break;
	case FG_DAEMON_CMD_GET_VBAT:
	{
		unsigned int vbat = 0;

		vbat = gauge_get_int_property(GAUGE_PROP_BATTERY_VOLTAGE) * 10;
		ret_msg->fgd_data_len += sizeof(vbat);
		memcpy(ret_msg->fgd_data, &vbat, sizeof(vbat));
		bm_debug("[K]FG_DAEMON_CMD_GET_VBAT = %d\n", vbat);
	}
	break;
	case FG_DAEMON_CMD_PRINT_LOG:
	{
		fg_cmd_check(msg);
		bm_debug("%s", &msg->fgd_data[0]);
	}
	break;
	case FG_DAEMON_CMD_SEND_CUSTOM_TABLE:
	{
		fg_daemon_send_data(gm, FG_DAEMON_CMD_SEND_CUSTOM_TABLE,
			&msg->fgd_data[0],
			&ret_msg->fgd_data[0]);
	}
	break;
	case FG_DAEMON_CMD_GET_CUSTOM_SETTING:
	case FG_DAEMON_CMD_GET_CUSTOM_TABLE:
	{
		fg_cmd_check(msg);
		bm_debug("[K]FG_DAEMON_CMD_GET_DATA\n");
		fg_daemon_get_data(msg->fgd_cmd, &msg->fgd_data[0],
			&ret_msg->fgd_data[0]);
		ret_msg->fgd_data_len =
			sizeof(struct fgd_cmd_param_t_4);
	}
	break;
	case FG_DAEMON_CMD_DUMP_LOG:
	{
		bm_debug("[K]FG_DAEMON_CMD_DUMP_LOG %d %d %d\n",
			msg->fgd_subcmd, msg->fgd_subcmd_para1,
			(int)strlen(&msg->fgd_data[0]));
	}
	break;
	case FG_DAEMON_CMD_GET_SHUTDOWN_CAR:
	{
		int shutdown_car_diff = 0;

		shutdown_car_diff = gauge_get_int_property(
			GAUGE_PROP_SHUTDOWN_CAR);
		ret_msg->fgd_data_len += sizeof(shutdown_car_diff);
		memcpy(ret_msg->fgd_data, &shutdown_car_diff,
			sizeof(shutdown_car_diff));
		bm_debug(
			"[K]FG_DAEMON_CMD_GET_SHUTDOWN_CAR = %d\n",
			shutdown_car_diff);
	}
	break;
	case FG_DAEMON_CMD_GET_NCAR:
	{
		int ver;

		ver = gauge_get_int_property(GAUGE_PROP_HW_VERSION);
		if (ver >= GAUGE_HW_V1000 &&
			ver < GAUGE_HW_V2000)
			memcpy(ret_msg->fgd_data, &gm->bat_cycle_ncar,
				sizeof(gm->bat_cycle_ncar));
		else {
			gauge_set_property(GAUGE_PROP_HW_INFO, 1);
			memcpy(ret_msg->fgd_data, &gm->gauge->fg_hw_info.ncar,
				sizeof(gm->gauge->fg_hw_info.ncar));
		}
		bm_debug("[K]FG_DAEMON_CMD_GET_NCAR version:%d ncar:%d %d\n",
			ver, gm->bat_cycle_ncar, gm->gauge->fg_hw_info.ncar);
	}
	break;
	case FG_DAEMON_CMD_GET_CURR_1:
	{
		gauge_set_property(GAUGE_PROP_HW_INFO, 1);

		memcpy(ret_msg->fgd_data, &gm->gauge->fg_hw_info.current_1,
			sizeof(gm->gauge->fg_hw_info.current_1));

		bm_debug("[K]FG_DAEMON_CMD_GET_CURR_1 %d\n",
			gm->gauge->fg_hw_info.current_1);
	}
	break;
	case FG_DAEMON_CMD_GET_CURR_2:
	{
		gauge_set_property(GAUGE_PROP_HW_INFO, 1);

		memcpy(ret_msg->fgd_data, &gm->gauge->fg_hw_info.current_2,
			sizeof(gm->gauge->fg_hw_info.current_2));

		bm_debug("[K]FG_DAEMON_CMD_GET_CURR_2 %d\n",
			gm->gauge->fg_hw_info.current_2);

	}
	break;
	case FG_DAEMON_CMD_GET_REFRESH:
	{
		gauge_set_property(GAUGE_PROP_HW_INFO, 1);
		bm_debug("[K]FG_DAEMON_CMD_GET_REFRESH\n");
	}
	break;
	case FG_DAEMON_CMD_GET_IS_AGING_RESET:
	{
		int reset = gm->is_reset_aging_factor;

		memcpy(ret_msg->fgd_data, &reset,
			sizeof(reset));

		gm->is_reset_aging_factor = 0;
		bm_debug("[K]FG_DAEMON_CMD_GET_IS_AGING_RESET %d %d\n",
			reset, gm->is_reset_aging_factor);
	}
	break;
	case FG_DAEMON_CMD_SET_SOC:
	{
		memcpy(&int_value, &msg->fgd_data[0], sizeof(int_value));
		gm->soc = (int_value + 50) / 100;
		bm_debug("[K]FG_DAEMON_CMD_SET_SOC %d\n",
			gm->soc);
	}
	break;
	case FG_DAEMON_CMD_SET_C_D0_SOC:
	{
		memcpy(&int_value, &msg->fgd_data[0], sizeof(int_value));
		gm->fg_cust_data.c_old_d0 = int_value;
		bm_debug("[K]FG_DAEMON_CMD_SET_C_D0_SOC %d\n",
		gm->fg_cust_data.c_old_d0);
	}
	break;
	case FG_DAEMON_CMD_SET_V_D0_SOC:
	{
		memcpy(&int_value, &msg->fgd_data[0], sizeof(int_value));
		gm->fg_cust_data.v_old_d0 = int_value;
		bm_debug("[K]FG_DAEMON_CMD_SET_V_D0_SOC %d\n",
		gm->fg_cust_data.v_old_d0);
	}
	break;
	case FG_DAEMON_CMD_SET_C_SOC:
	{
		memcpy(&int_value, &msg->fgd_data[0], sizeof(int_value));
		gm->fg_cust_data.c_soc = int_value;
		bm_debug("[K]FG_DAEMON_CMD_SET_C_SOC %d\n",
		gm->fg_cust_data.c_soc);
	}
	break;
	case FG_DAEMON_CMD_SET_V_SOC:
	{
		memcpy(&int_value, &msg->fgd_data[0], sizeof(int_value));
		gm->fg_cust_data.v_soc = int_value;
		bm_debug("[K]FG_DAEMON_CMD_SET_V_SOC %d\n",
		gm->fg_cust_data.v_soc);
	}
	break;
	case FG_DAEMON_CMD_SET_QMAX_T_AGING:
	{
		/* todo */
		bm_debug("[K]FG_DAEMON_CMD_SET_QMAX_T_AGING\n");
	}
	break;
	case FG_DAEMON_CMD_SET_SAVED_CAR:
	{
		memcpy(&int_value, &msg->fgd_data[0], sizeof(int_value));
		gm->d_saved_car = int_value;
		bm_debug("[K]FG_DAEMON_CMD_SET_SAVED_CAR %d\n",
		gm->d_saved_car);
	}
	break;
	case FG_DAEMON_CMD_SET_AGING_FACTOR:
	{
		memcpy(&int_value, &msg->fgd_data[0], sizeof(int_value));
		gm->aging_factor = int_value;
		bm_debug("[K]FG_DAEMON_CMD_SET_AGING_FACTOR %d\n",
		gm->aging_factor);
	}
	break;
	case FG_DAEMON_CMD_SET_QMAX:
	{
		memcpy(&int_value, &msg->fgd_data[0], sizeof(int_value));
		gm->algo_qmax = int_value;
		bm_debug("[K]FG_DAEMON_CMD_SET_QMAX %d\n",
		gm->algo_qmax);
	}
	break;
	case FG_DAEMON_CMD_SET_BAT_CYCLES:
	{
		memcpy(&int_value, &msg->fgd_data[0], sizeof(int_value));
		gm->bat_cycle = int_value;
		bm_debug("[K]FG_DAEMON_CMD_SET_BAT_CYCLES %d\n",
		gm->bat_cycle);
	}
	break;
	case FG_DAEMON_CMD_SET_NCAR:
	{
		memcpy(&int_value, &msg->fgd_data[0], sizeof(int_value));
		gm->bat_cycle_ncar = int_value;
		bm_debug("[K]FG_DAEMON_CMD_SET_NCAR %d\n",
		gm->bat_cycle_ncar);
	}
	break;
	case FG_DAEMON_CMD_SET_OCV_MAH:
	{
		memcpy(&int_value, &msg->fgd_data[0], sizeof(int_value));
		gm->algo_ocv_to_mah = int_value;
		bm_debug("[K]FG_DAEMON_CMD_SET_OCV_MAH %d\n",
		gm->algo_ocv_to_mah);
	}
	break;
	case FG_DAEMON_CMD_SET_OCV_VTEMP:
	{
		memcpy(&int_value, &msg->fgd_data[0], sizeof(int_value));
		gm->algo_vtemp = int_value;
		bm_debug("[K]FG_DAEMON_CMD_SET_OCV_VTEMP %d\n",
		gm->algo_vtemp);
	}
	break;
	case FG_DAEMON_CMD_SET_OCV_SOC:
	{
		memcpy(&int_value, &msg->fgd_data[0], sizeof(int_value));
		gm->algo_ocv_to_soc = int_value;
		bm_debug("[K]FG_DAEMON_CMD_SET_OCV_SOC %d\n",
		gm->algo_ocv_to_soc);
	}
	break;
	case FG_DAEMON_CMD_SET_CON0_SOFF_VALID:
	{
		int ori_value = 0;

		memcpy(&int_value, &msg->fgd_data[0], sizeof(int_value));

		ori_value = gauge_get_int_property(
			GAUGE_PROP_MONITOR_SOFF_VALIDTIME);
		gauge_set_property(GAUGE_PROP_MONITOR_SOFF_VALIDTIME,
			int_value);

		bm_debug("[K]FG_DAEMON_CMD_SET_CON0_SOFF_VALID ori:%d, new:%d\n",
			ori_value, int_value);
	}
	break;

	default:
		badcmd++;
		bm_err("[%s]bad cmd:0x%x times:%d\n", __func__,
			msg->fgd_cmd, badcmd);
		break;
	}
}

static void mtk_battery_netlink_handler(struct sk_buff *skb)
{
	u32 pid;
	kuid_t uid;
	int seq;
	void *data;
	struct nlmsghdr *nlh;
	struct fgd_nl_msg_t *fgd_msg, *fgd_ret_msg;
	int size = 0;

	nlh = (struct nlmsghdr *)skb->data;
	pid = NETLINK_CREDS(skb)->pid;
	uid = NETLINK_CREDS(skb)->uid;
	seq = nlh->nlmsg_seq;

	data = NLMSG_DATA(nlh);

	fgd_msg = (struct fgd_nl_msg_t *)data;


	bm_debug("rcv:%d %d %d %d %d %d\n",
		fgd_msg->nl_cmd,
		fgd_msg->fgd_cmd,
		fgd_msg->fgd_subcmd,
		fgd_msg->fgd_subcmd_para1,
		fgd_msg->fgd_data_len,
		fgd_msg->identity);

	if (fgd_msg->identity != FGD_NL_MAGIC) {
		bm_err("[%s]not correct MTKFG netlink packet!%d\n",
			__func__, fgd_msg->identity);
		return;
	}

	if (g_fgd_pid != pid &&
		fgd_msg->fgd_cmd > FG_DAEMON_CMD_SET_DAEMON_PID) {
		bm_err("[%s]drop rev netlink pid:%d:%d  cmd:%d:%d\n",
			__func__,
			pid,
			g_fgd_pid,
			fgd_msg->fgd_cmd,
			FG_DAEMON_CMD_SET_DAEMON_PID);
		return;
	}


	size = fgd_msg->fgd_ret_data_len + FGD_NL_HDR_LEN;

	if (size > (PAGE_SIZE << 1))
		fgd_ret_msg = vmalloc(size);
	else {
		if (in_interrupt())
			fgd_ret_msg = kmalloc(size, GFP_ATOMIC);
		else
			fgd_ret_msg = kmalloc(size, GFP_KERNEL);
	}

	if (fgd_ret_msg == NULL) {
		if (size > PAGE_SIZE)
			fgd_ret_msg = vmalloc(size);

		if (fgd_ret_msg == NULL)
			return;
	}

	if (!fgd_ret_msg)
		return;

	memset(fgd_ret_msg, 0, size);
	mtk_battery_daemon_handler(data, fgd_ret_msg);
	mtk_battery_send_to_user(seq, fgd_ret_msg);

	kvfree(fgd_ret_msg);
}

unsigned int TempConverBattThermistor(struct mtk_battery *gm, int temp)
{
	int RES1 = 0, RES2 = 0;
	int TMP1 = 0, TMP2 = 0;
	int i;
	unsigned int TBatt_R_Value = 0xffff;
	struct fuelgauge_temperature *ptable;

	ptable = gm->tmp_table;


	if (temp >= ptable[20].BatteryTemp) {
		TBatt_R_Value = ptable[20].TemperatureR;
	} else if (temp <= ptable[0].BatteryTemp) {
		TBatt_R_Value = ptable[0].TemperatureR;
	} else {
		RES1 = ptable[0].TemperatureR;
		TMP1 = ptable[0].BatteryTemp;

		for (i = 0; i <= 20; i++) {
			if (temp <= ptable[i].BatteryTemp) {
				RES2 = ptable[i].TemperatureR;
				TMP2 = ptable[i].BatteryTemp;
				break;
			}
			{	/* hidden else */
				RES1 = ptable[i].TemperatureR;
				TMP1 = ptable[i].BatteryTemp;
			}
		}


		TBatt_R_Value = interpolation(TMP1, RES1, TMP2, RES2, temp);
	}

	bm_trace(
		"[%s] [%d] %d %d %d %d %d\n",
		__func__,
		TBatt_R_Value, TMP1,
		RES1, TMP2, RES2, temp);

	return TBatt_R_Value;
}

unsigned int TempToBattVolt(struct mtk_battery *gm, int temp, int update)
{
	unsigned int R_NTC = TempConverBattThermistor(gm, temp);
	long long Vin = 0;
	long long V_IR_comp = 0;
	/*int vbif28 = pmic_get_auxadc_value(AUXADC_LIST_VBIF);*/
	int vbif28 = gm->rbat.rbat_pull_up_volt;
	static int fg_current_temp;
	static bool fg_current_state;
	int fg_r_value = gm->fg_cust_data.com_r_fg_value;
	int fg_meter_res_value = 0;

	if (gm->no_bat_temp_compensate == 0)
		fg_meter_res_value = gm->fg_cust_data.com_fg_meter_resistance;
	else
		fg_meter_res_value = 0;

#ifdef RBAT_PULL_UP_VOLT_BY_BIF
	vbif28 = gauge_get_int_property(GAUGE_PROP_BIF_VOLTAGE);
#endif
	Vin = (long long)R_NTC * vbif28 * 10;	/* 0.1 mV */

#if defined(__LP64__) || defined(_LP64)
	do_div(Vin, (R_NTC + gm->rbat.rbat_pull_up_r));
#else
	Vin = div_s64(Vin, (R_NTC + gm->rbat.rbat_pull_up_r));
#endif

	if (update == true) {
		fg_current_temp = gauge_get_int_property(
			GAUGE_PROP_BATTERY_CURRENT);
		if (fg_current_temp <= 0)
			fg_current_state = false;
		else
			fg_current_state = true;
	}

	if (fg_current_state == true) {
		V_IR_comp = Vin;
		V_IR_comp +=
			((fg_current_temp *
			(fg_meter_res_value + fg_r_value)) / 10000);
	} else {
		V_IR_comp = Vin;
		V_IR_comp -=
			((fg_current_temp *
			(fg_meter_res_value + fg_r_value)) / 10000);
	}

	bm_trace("[%s] temp %d R_NTC %d V(%lld %lld) I %d CHG %d\n",
		__func__,
		temp, R_NTC, Vin, V_IR_comp, fg_current_temp, fg_current_state);

	return (unsigned int) V_IR_comp;
}


void fg_bat_temp_int_internal(struct mtk_battery *gm)
{
	int tmp = 0;
	int fg_bat_new_ht, fg_bat_new_lt;

	if (gm->disableGM30) {
		gm->bs_data.bat_batt_temp = 25;
		battery_update(gm);
		return;
	}

#if defined(CONFIG_MTK_DISABLE_GAUGE) || defined(FIXED_TBAT_25)
	gm->bs_data.bat_batt_temp = 25;
	battery_update(gm);
	tmp = 1;
	fg_bat_new_ht = 1;
	fg_bat_new_lt = 1;
	return;
#else
	tmp = force_get_tbat(gm, true);

	gauge_set_property(GAUGE_PROP_EN_BAT_TMP_LT, 0);
	gauge_set_property(GAUGE_PROP_EN_BAT_TMP_HT, 0);


//	if (get_ec()->fixed_temp_en == 1)
//		tmp = get_ec()->fixed_temp_value;

	if (tmp >= gm->bat_tmp_c_ht)
		wakeup_fg_algo(gm, FG_INTR_BAT_TMP_C_HT);
	else if (tmp <= gm->bat_tmp_c_lt)
		wakeup_fg_algo(gm, FG_INTR_BAT_TMP_C_LT);

	if (tmp >= gm->bat_tmp_ht)
		wakeup_fg_algo(gm, FG_INTR_BAT_TMP_HT);
	else if (tmp <= gm->bat_tmp_lt)
		wakeup_fg_algo(gm, FG_INTR_BAT_TMP_LT);

	fg_bat_new_ht = TempToBattVolt(gm, tmp + 1, 1);
	fg_bat_new_lt = TempToBattVolt(gm, tmp - 1, 0);

	if (gm->fixed_bat_tmp == 0xffff) {
		gauge_set_property(GAUGE_PROP_BAT_TMP_LT_THRESHOLD,
			fg_bat_new_lt);
		gauge_set_property(GAUGE_PROP_EN_BAT_TMP_LT, 1);

		gauge_set_property(GAUGE_PROP_BAT_TMP_HT_THRESHOLD,
			fg_bat_new_ht);
		gauge_set_property(GAUGE_PROP_EN_BAT_TMP_HT, 1);
	}
	bm_debug("[%s][FG_TEMP_INT] T[%d] V[%d %d] C[%d %d] h[%d %d]\n",
		__func__,
		tmp, gm->bat_tmp_ht,
		gm->bat_tmp_lt, gm->bat_tmp_c_ht,
		gm->bat_tmp_c_lt,
		fg_bat_new_lt, fg_bat_new_ht);

	gm->bs_data.bat_batt_temp = tmp;
	battery_update(gm);
#endif
}

void fg_bat_temp_int_sw_check(struct mtk_battery *gm)
{
	int tmp = force_get_tbat(gm, true);

	if (gm->disableGM30)
		return;

	bm_debug(
		"[%s] tmp %d lt %d ht %d\n",
		__func__,
		tmp, gm->bat_tmp_lt,
		gm->bat_tmp_ht);

	if (tmp >= gm->bat_tmp_ht)
		fg_bat_temp_int_internal(gm);
	else if (tmp <= gm->bat_tmp_lt)
		fg_bat_temp_int_internal(gm);
}

void fg_int_event(struct mtk_battery *gm, enum gauge_event evt)
{
	if (evt != EVT_INT_NAFG_CHECK)
		fg_bat_temp_int_sw_check(gm);

	gauge_set_property(GAUGE_PROP_EVENT, evt);
}

/* ============================================================ */
/* check bat plug out  */
/* ============================================================ */
void sw_check_bat_plugout(struct mtk_battery *gm)
{
	int is_bat_exist;

	if (gm->disable_plug_int && gm->disableGM30 != true) {
		is_bat_exist = gauge_get_int_property(GAUGE_PROP_BATTERY_EXIST);
		/* fg_bat_plugout_int_handler(); */
		if (is_bat_exist == 0) {
			bm_err(
				"[swcheck_bat_plugout]g_disable_plug_int=%d, is_bat_exist %d, is_fg_disable %d\n",
				gm->disable_plug_int,
				is_bat_exist,
				gm->disableGM30);

			gm->bs_data.bat_status = POWER_SUPPLY_STATUS_UNKNOWN;
			wakeup_fg_algo(gm, FG_INTR_BAT_PLUGOUT);
			battery_update(gm);
			kernel_power_off();
		}
	}
}

/* ============================================================ */
/* hw low battery interrupt handler */
/* ============================================================ */
static int sw_vbat_h_irq_handler(struct mtk_battery *gm)
{
	bm_debug("[%s]\n", __func__);
	gauge_set_property(GAUGE_PROP_EN_HIGH_VBAT_INTERRUPT, 0);
	gauge_set_property(GAUGE_PROP_EN_LOW_VBAT_INTERRUPT, 0);
	disable_shutdown_cond(gm, LOW_BAT_VOLT);
	wakeup_fg_algo(gm, FG_INTR_VBAT2_H);

	fg_int_event(gm, EVT_INT_VBAT_H);
	sw_check_bat_plugout(gm);
	return 0;
}

static irqreturn_t vbat_h_irq(int irq, void *data)
{
	struct mtk_battery *gm = data;

	if (fg_interrupt_check(gm) == false)
		return IRQ_HANDLED;
	sw_vbat_h_irq_handler(gm);
	return IRQ_HANDLED;
}

static int sw_vbat_l_irq_handler(struct mtk_battery *gm)
{
	bm_debug("[%s]\n", __func__);
	gauge_set_property(GAUGE_PROP_EN_HIGH_VBAT_INTERRUPT, 0);
	gauge_set_property(GAUGE_PROP_EN_LOW_VBAT_INTERRUPT, 0);
	wakeup_fg_algo(gm, FG_INTR_VBAT2_L);

	fg_int_event(gm, EVT_INT_VBAT_L);
	sw_check_bat_plugout(gm);
	return 0;
}

static irqreturn_t vbat_l_irq(int irq, void *data)
{
	struct mtk_battery *gm = data;

	if (fg_interrupt_check(gm) == false)
		return IRQ_HANDLED;
	sw_vbat_l_irq_handler(gm);

	return IRQ_HANDLED;
}

/* ============================================================ */
/* nafg interrupt handler */
/* ============================================================ */
static int nafg_irq_handler(struct mtk_battery *gm)
{
	struct mtk_gauge *gauge;
	int nafg_en = 0;
	signed int nafg_cnt = 0;
	signed int nafg_dltv = 0;
	signed int nafg_c_dltv = 0;

	gauge = gm->gauge;
	/* 1. Get SW Car value */
	fg_int_event(gm, EVT_INT_NAFG_CHECK);

	nafg_cnt = gauge_get_int_property(GAUGE_PROP_NAFG_CNT);
	nafg_dltv = gauge_get_int_property(GAUGE_PROP_NAFG_DLTV);
	nafg_c_dltv = gauge_get_int_property(GAUGE_PROP_NAFG_C_DLTV);

	gauge->hw_status.nafg_cnt = nafg_cnt;
	gauge->hw_status.nafg_dltv = nafg_dltv;
	gauge->hw_status.nafg_c_dltv = nafg_c_dltv;

	bm_err(
		"[%s][cnt:%d dltv:%d cdltv:%d cdltvt:%d zcv:%d vbat:%d]\n",
		__func__,
		nafg_cnt,
		nafg_dltv,
		nafg_c_dltv,
		gauge->hw_status.nafg_c_dltv_th,
		gauge->hw_status.nafg_zcv,
		gauge_get_int_property(GAUGE_PROP_BATTERY_VOLTAGE));
	/* battery_dump_nag(); */

	/* 2. Stop HW interrupt*/
	gauge_set_nag_en(gm, nafg_en);

	fg_int_event(gm, EVT_INT_NAFG);

	/* 3. Notify fg daemon */
	wakeup_fg_algo(gm, FG_INTR_NAG_C_DLTV);
	get_monotonic_boottime(&gm->last_nafg_update_time);
	return 0;
}

static irqreturn_t nafg_irq(int irq, void *data)
{
	struct mtk_battery *gm = data;

	if (fg_interrupt_check(gm) == false)
		return IRQ_HANDLED;
	nafg_irq_handler(gm);
	return IRQ_HANDLED;
}

/* ============================================================ */
/* battery plug out interrupt handler */
/* ============================================================ */
static irqreturn_t bat_plugout_irq(int irq, void *data)
{
	struct mtk_battery *gm = data;
	int is_bat_exist;

	is_bat_exist = gauge_get_int_property(GAUGE_PROP_BATTERY_EXIST);

	bm_err("[%s]is_bat %d miss:%d\n",
		__func__,
		is_bat_exist, gm->plug_miss_count);

	if (fg_interrupt_check(gm) == false)
		return IRQ_HANDLED;

	/* avoid battery plug status mismatch case*/
	if (is_bat_exist == 1) {
		fg_int_event(gm, EVT_INT_BAT_PLUGOUT);
		gm->plug_miss_count++;

		bm_err("[%s]is_bat %d miss:%d\n",
			__func__,
			is_bat_exist, gm->plug_miss_count);

		if (gm->plug_miss_count >= 3) {
			disable_gauge_irq(gm->gauge, BAT_PLUGOUT_IRQ);
			bm_err("[%s]disable FG_BAT_PLUGOUT\n",
				__func__);
			gm->disable_plug_int = 1;
		}
	}

	if (is_bat_exist == 0) {
		gm->bs_data.bat_status = POWER_SUPPLY_STATUS_UNKNOWN;
		wakeup_fg_algo(gm, FG_INTR_BAT_PLUGOUT);
		battery_update(gm);
		fg_int_event(gm, EVT_INT_BAT_PLUGOUT);
		kernel_power_off();
	}
	return IRQ_HANDLED;
}

/* ============================================================ */
/* zcv interrupt handler */
/* ============================================================ */
static irqreturn_t zcv_irq(int irq, void *data)
{
	struct mtk_battery *gm = data;
	int fg_coulomb = 0;
	int zcv_intr_en = 0;
	int zcv_intr_curr = 0;
	int zcv;

	if (fg_interrupt_check(gm) == false)
		return IRQ_HANDLED;

	fg_coulomb = gauge_get_int_property(GAUGE_PROP_COULOMB);
	zcv_intr_curr = gauge_get_int_property(GAUGE_PROP_ZCV_CURRENT);
	zcv = gauge_get_int_property(GAUGE_PROP_ZCV);
	bm_err("[%s] car:%d zcv_curr:%d zcv:%d, slp_cur_avg:%d\n",
		__func__,
		fg_coulomb, zcv_intr_curr, zcv,
		gm->fg_cust_data.sleep_current_avg);

	if (abs(zcv_intr_curr) < gm->fg_cust_data.sleep_current_avg) {
		wakeup_fg_algo(gm, FG_INTR_FG_ZCV);
		zcv_intr_en = 0;
		gauge_set_property(GAUGE_PROP_ZCV_INTR_EN, zcv_intr_en);
	}

	fg_int_event(gm, EVT_INT_ZCV);
	sw_check_bat_plugout(gm);
	return IRQ_HANDLED;
}

/* ============================================================ */
/* battery cycle interrupt handler */
/* ============================================================ */
void fg_sw_bat_cycle_accu(struct mtk_battery *gm)
{
	int diff_car = 0, tmp_car = 0, tmp_ncar = 0, tmp_thr = 0;
	int fg_coulomb = 0, gauge_version;

	fg_coulomb = gauge_get_int_property(GAUGE_PROP_COULOMB);
	tmp_car = gm->bat_cycle_car;
	tmp_ncar = gm->bat_cycle_ncar;

	diff_car = fg_coulomb - gm->bat_cycle_car;

	if (diff_car > 0) {
		bm_err("[%s]ERROR!drop diff_car\n", __func__);
		gm->bat_cycle_car = fg_coulomb;
	} else {
		gm->bat_cycle_ncar = gm->bat_cycle_ncar + abs(diff_car);
		gm->bat_cycle_car = fg_coulomb;
	}

	gauge_set_property(GAUGE_PROP_HW_INFO, 0);
	bm_err("[%s]car[o:%d n:%d],diff_car:%d,ncar[o:%d n:%d hw:%d] thr %d\n",
		__func__,
		tmp_car, fg_coulomb, diff_car,
		tmp_ncar, gm->bat_cycle_ncar, gm->gauge->fg_hw_info.ncar,
		gm->bat_cycle_thr);

	gauge_version = gauge_get_int_property(GAUGE_PROP_HW_VERSION);
	if (gauge_version >= GAUGE_HW_V1000 &&
		gauge_version < GAUGE_HW_V2000) {
		if (gm->bat_cycle_thr > 0 &&
			gm->bat_cycle_ncar >= gm->bat_cycle_thr) {
			tmp_ncar = gm->bat_cycle_ncar;
			tmp_thr = gm->bat_cycle_thr;

			gm->bat_cycle_ncar = 0;
			wakeup_fg_algo(gm, FG_INTR_BAT_CYCLE);
			bm_err("[fg_cycle_int_handler] ncar:%d thr:%d\n",
				tmp_ncar, tmp_thr);
		}
	}
}

static irqreturn_t cycle_irq(int irq, void *data)
{
	struct mtk_battery *gm = data;

	if (fg_interrupt_check(gm) == false)
		return IRQ_HANDLED;

	disable_gauge_irq(gm->gauge, FG_N_CHARGE_L_IRQ);
	wakeup_fg_algo(gm, FG_INTR_BAT_CYCLE);
	fg_int_event(gm, EVT_INT_BAT_CYCLE);
	sw_check_bat_plugout(gm);

	return IRQ_HANDLED;
}

/* ============================================================ */
/* hw iavg interrupt handler */
/* ============================================================ */
static irqreturn_t iavg_h_irq(int irq, void *data)
{
	struct mtk_battery *gm = data;

	if (fg_interrupt_check(gm) == false)
		return IRQ_HANDLED;
	gm->gauge->hw_status.iavg_intr_flag = 0;
	disable_gauge_irq(gm->gauge, FG_IAVG_H_IRQ);
	disable_gauge_irq(gm->gauge, FG_IAVG_L_IRQ);

	bm_debug("[%s]iavg_intr_flag %d\n",
		__func__,
		gm->gauge->hw_status.iavg_intr_flag);
	wakeup_fg_algo(gm, FG_INTR_IAVG);
	fg_int_event(gm, EVT_INT_IAVG);
	sw_check_bat_plugout(gm);
	return IRQ_HANDLED;
}

static irqreturn_t iavg_l_irq(int irq, void *data)
{
	struct mtk_battery *gm = data;

	if (fg_interrupt_check(gm) == false)
		return IRQ_HANDLED;
	gm->gauge->hw_status.iavg_intr_flag = 0;
	disable_gauge_irq(gm->gauge, FG_IAVG_H_IRQ);
	disable_gauge_irq(gm->gauge, FG_IAVG_L_IRQ);

	bm_debug("[%s]iavg_intr_flag %d\n",
		__func__,
		gm->gauge->hw_status.iavg_intr_flag);
	wakeup_fg_algo(gm, FG_INTR_IAVG);
	fg_int_event(gm, EVT_INT_IAVG);
	sw_check_bat_plugout(gm);
	return IRQ_HANDLED;
}

/* ============================================================ */
/* bat temp interrupt handler */
/* ============================================================ */
static irqreturn_t bat_temp_irq(int irq, void *data)
{
	struct mtk_battery *gm = data;

	if (fg_interrupt_check(gm) == false)
		return IRQ_HANDLED;

	bm_debug("[%s]\n", __func__);
	fg_bat_temp_int_internal(gm);

	return IRQ_HANDLED;
}

/* ============================================================ */
/* sw iavg */
/* ============================================================ */
static void sw_iavg_init(struct mtk_battery *gm)
{
	int bat_current = 0;

	get_monotonic_boottime(&gm->sw_iavg_time);
	gm->sw_iavg_car = gauge_get_int_property(GAUGE_PROP_COULOMB);

	/* BAT_DISCHARGING = 0 */
	/* BAT_CHARGING = 1 */
	bat_current = gauge_get_int_property(GAUGE_PROP_BATTERY_CURRENT);

	gm->sw_iavg = bat_current;

	gm->sw_iavg_ht = gm->sw_iavg + gm->sw_iavg_gap;
	gm->sw_iavg_lt = gm->sw_iavg - gm->sw_iavg_gap;

	bm_debug("%s %d\n", __func__, gm->sw_iavg);
}

void fg_update_sw_iavg(struct mtk_battery *gm)
{
	struct timespec now_time, diff;
	int fg_coulomb;
	int version;

	get_monotonic_boottime(&now_time);

	diff = timespec_sub(now_time, gm->sw_iavg_time);
	bm_debug("[%s]diff time:%ld\n",
		__func__,
		diff.tv_sec);
	if (diff.tv_sec >= 60) {
		fg_coulomb = gauge_get_int_property(GAUGE_PROP_COULOMB);
		gm->sw_iavg = (fg_coulomb - gm->sw_iavg_car)
			* 3600 / diff.tv_sec;
		gm->sw_iavg_time = now_time;
		gm->sw_iavg_car = fg_coulomb;
		version = gauge_get_int_property(GAUGE_PROP_HW_VERSION);
		if (gm->sw_iavg >= gm->sw_iavg_ht
			|| gm->sw_iavg <= gm->sw_iavg_lt) {
			gm->sw_iavg_ht = gm->sw_iavg + gm->sw_iavg_gap;
			gm->sw_iavg_lt = gm->sw_iavg - gm->sw_iavg_gap;
			if (version < GAUGE_HW_V2000)
				wakeup_fg_algo(gm, FG_INTR_IAVG);
		}
		bm_debug("[%s]time:%ld car:%d %d iavg:%d ht:%d lt:%d gap:%d\n",
			__func__,
			diff.tv_sec, fg_coulomb, gm->sw_iavg_car, gm->sw_iavg,
			gm->sw_iavg_ht, gm->sw_iavg_lt, gm->sw_iavg_gap);
	}

}

int wakeup_fg_daemon(unsigned int flow_state, int cmd, int para1)
{
	if (g_fgd_pid != 0) {
		struct fgd_nl_msg_t *fgd_msg;
		int size = FGD_NL_HDR_LEN + sizeof(flow_state);

		if (size > (PAGE_SIZE << 1))
			fgd_msg = vmalloc(size);
		else {
			if (in_interrupt())
				fgd_msg = kmalloc(size, GFP_ATOMIC);
			else
				fgd_msg = kmalloc(size, GFP_KERNEL);
		}

		if (fgd_msg == NULL) {
			if (size > PAGE_SIZE)
				fgd_msg = vmalloc(size);

			if (fgd_msg == NULL)
				return -1;
		}

		bm_debug("[%s] malloc size=%d pid=%d cmd:%d\n",
			__func__,
			size, g_fgd_pid, flow_state);
		memset(fgd_msg, 0, size);
		fgd_msg->fgd_cmd = FG_DAEMON_CMD_NOTIFY_DAEMON;
		memcpy(fgd_msg->fgd_data, &flow_state, sizeof(flow_state));
		fgd_msg->fgd_data_len += sizeof(flow_state);
		mtk_battery_send_to_user(0, fgd_msg);

		kvfree(fgd_msg);

		return 0;
	} else
		return -1;

}

void fg_drv_update_daemon(struct mtk_battery *gm)
{
	fg_update_sw_iavg(gm);
}

static void mtk_battery_shutdown(struct mtk_battery *gm)
{
	int fg_coulomb = 0;
	int shut_car_diff = 0;
	int verify_car;

	fg_coulomb = gauge_get_int_property(GAUGE_PROP_COULOMB);
	if (gm->d_saved_car != 0) {
		shut_car_diff = fg_coulomb - gm->d_saved_car;
		gauge_set_property(GAUGE_PROP_SHUTDOWN_CAR, shut_car_diff);
		/* ready for writing to PMIC_RG */
	}
	verify_car = gauge_get_int_property(GAUGE_PROP_SHUTDOWN_CAR);

	bm_err("******** %s!! car=[o:%d,new:%d,diff:%d v:%d]********\n",
		__func__,
		gm->d_saved_car, fg_coulomb, shut_car_diff, verify_car);

}

void enable_bat_temp_det(bool en)
{
	/* todo in ALSP */
}

static int mtk_battery_suspend(struct mtk_battery *gm, pm_message_t state)
{
	int version;

	bm_err("******** %s!! iavg=%d ***GM3 disable:%d %d %d %d tmp_intr:%d***\n",
		__func__,
		gm->gauge->hw_status.iavg_intr_flag,
		gm->disableGM30,
		gm->fg_cust_data.disable_nafg,
		gm->ntc_disable_nafg,
		gm->cmd_disable_nafg,
		gm->enable_tmp_intr_suspend);

	if (gm->enable_tmp_intr_suspend == 0)
		enable_bat_temp_det(0);

	version = gauge_get_int_property(GAUGE_PROP_HW_VERSION);

	if (version >= GAUGE_HW_V2000
		&& gm->gauge->hw_status.iavg_intr_flag == 1) {
		disable_gauge_irq(gm->gauge, FG_IAVG_H_IRQ);
		if (gm->gauge->hw_status.iavg_lt > 0)
			disable_gauge_irq(gm->gauge, FG_IAVG_L_IRQ);
	}
	return 0;
}

static int mtk_battery_resume(struct mtk_battery *gm)
{
	int version;

	bm_err("******** %s!! iavg=%d ***GM3 disable:%d %d %d %d***\n",
		__func__,
		gm->gauge->hw_status.iavg_intr_flag,
		gm->disableGM30,
		gm->fg_cust_data.disable_nafg,
		gm->ntc_disable_nafg,
		gm->cmd_disable_nafg);

	version = gauge_get_int_property(GAUGE_PROP_HW_VERSION);
	if (version >=
		GAUGE_HW_V2000
		&& gm->gauge->hw_status.iavg_intr_flag == 1) {
		enable_gauge_irq(gm->gauge, FG_IAVG_H_IRQ);
		if (gm->gauge->hw_status.iavg_lt > 0)
			enable_gauge_irq(gm->gauge, FG_IAVG_L_IRQ);
	}
	/* reset nafg monitor time to avoid suspend for too long case */
	get_monotonic_boottime(&gm->last_nafg_update_time);

	fg_update_sw_iavg(gm);

	enable_bat_temp_det(1);

	return 0;
}

bool is_daemon_support(struct mtk_battery *gm)
{
	pr_notice("%s: CONFIG_NET = false\n", __func__);
	return false;
}

int mtk_battery_daemon_init(struct platform_device *pdev)
{
	int ret;
	int hw_version;
	struct mtk_battery *gm;
	struct mtk_gauge *gauge;
	struct netlink_kernel_cfg cfg = {
		.input = mtk_battery_netlink_handler,
	};

	gauge = dev_get_drvdata(&pdev->dev);
	gm = gauge->gm;

	if (is_daemon_support(gm) == false)
		return -EIO;

	mtk_battery_sk = netlink_kernel_create(&init_net, NETLINK_FGD, &cfg);
	bm_debug("[%s]netlink_kernel_create protol= %d\n",
		__func__, NETLINK_FGD);

	if (mtk_battery_sk == NULL) {
		bm_err("netlink_kernel_create error\n");
		return -EIO;
	}
	bm_err("[%s]netlink_kernel_create ok\n", __func__);

	gm->pl_two_sec_reboot = gauge_get_int_property(GAUGE_PROP_2SEC_REBOOT);
	gauge_set_property(GAUGE_PROP_2SEC_REBOOT, 0);

	hw_version = gauge_get_int_property(GAUGE_PROP_HW_VERSION);

	gm->shutdown = mtk_battery_shutdown;
	gm->suspend = mtk_battery_suspend;
	gm->resume = mtk_battery_resume;

	if (hw_version == GAUGE_NO_HW) {
		gm->gauge->sw_nafg_irq = nafg_irq_handler;
		gm->gauge->sw_vbat_h_irq = sw_vbat_h_irq_handler;
		gm->gauge->sw_vbat_l_irq = sw_vbat_l_irq_handler;
	}

	if (hw_version >= GAUGE_HW_V0500) {
		ret = devm_request_threaded_irq(&gm->gauge->pdev->dev,
		gm->gauge->irq_no[ZCV_IRQ],
		NULL, zcv_irq,
		IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
		"mtk_gauge_zcv",
		gm);

		if (hw_version == GAUGE_HW_V0500) {
			ret = devm_request_threaded_irq(&gm->gauge->pdev->dev,
			gm->gauge->irq_no[VBAT_H_IRQ],
			NULL, vbat_h_irq,
			IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
			"mtk_gauge_vbat_high",
			gm);

			ret = devm_request_threaded_irq(&gm->gauge->pdev->dev,
			gm->gauge->irq_no[VBAT_L_IRQ],
			NULL, vbat_l_irq,
			IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
			"mtk_gauge_vbat_low",
			gm);
		}
	}

	if (hw_version >= GAUGE_HW_V1000) {
		ret = devm_request_threaded_irq(&gm->gauge->pdev->dev,
		gm->gauge->irq_no[NAFG_IRQ],
		NULL, nafg_irq,
		IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
		"mtk_gauge_nafg",
		gm);
	}

	if (hw_version >= GAUGE_HW_V2000) {
		ret = devm_request_threaded_irq(&gm->gauge->pdev->dev,
		gm->gauge->irq_no[BAT_PLUGOUT_IRQ],
		NULL, bat_plugout_irq,
		IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
		"mtk_gauge_bat_plugout",
		gm);

		ret = devm_request_threaded_irq(&gm->gauge->pdev->dev,
		gm->gauge->irq_no[FG_N_CHARGE_L_IRQ],
		NULL, cycle_irq,
		IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
		"mtk_cycle_zcv",
		gm);

		ret = devm_request_threaded_irq(&gm->gauge->pdev->dev,
		gm->gauge->irq_no[FG_IAVG_H_IRQ],
		NULL, iavg_h_irq,
		IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
		"mtk_gauge_iavg_h",
		gm);

		ret = devm_request_threaded_irq(&gm->gauge->pdev->dev,
		gm->gauge->irq_no[FG_IAVG_L_IRQ],
		NULL, iavg_l_irq,
		IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
		"mtk_gauge_iavg_l",
		gm);

		ret = devm_request_threaded_irq(&gm->gauge->pdev->dev,
		gm->gauge->irq_no[BAT_TMP_H_IRQ],
		NULL, bat_temp_irq,
		IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
		"mtk_bat_tmp_h",
		gm);

		ret = devm_request_threaded_irq(&gm->gauge->pdev->dev,
		gm->gauge->irq_no[BAT_TMP_L_IRQ],
		NULL, bat_temp_irq,
		IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
		"mtk_bat_tmp_l",
		gm);
	}

	sw_iavg_init(gm);

	return 0;
}

