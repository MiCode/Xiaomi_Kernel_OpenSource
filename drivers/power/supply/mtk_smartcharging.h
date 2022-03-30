/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_SMARTCHARGING_H
#define _MTK_SMARTCHARGING_H

#include <linux/netlink.h>	/* netlink */
#include <linux/socket.h>	/* netlink */
#include <linux/skbuff.h>	/* netlink */
#include <net/sock.h>		/* netlink */
#include "mtk_charger.h"

#define NETLINK_CHG 28

#define SCD_NL_MSG_T_HDR_LEN 28
#define MAX_NL_MSG_LEN_SND 4096
#define MAX_NL_MSG_LEN_RCV 9200
#define SCD_NL_MSG_MAX_LEN 9200

#define SCD_NL_MAGIC 19800212

struct mtk_charger;
struct charger_data;

enum sc_daemon_cmds {
	SC_DAEMON_CMD_PRINT_LOG,
	SC_DAEMON_CMD_SET_DAEMON_PID,
	SC_DAEMON_CMD_NOTIFY_DAEMON,
	SC_DAEMON_CMD_SETTING,

	SC_DAEMON_CMD_FROM_USER_NUMBER
};


enum sc_kernel_events {
	SC_EVENT_PLUG_IN,
	SC_EVENT_PLUG_OUT,
	SC_EVENT_CHARGING,
	SC_EVENT_STOP_CHARGING,
};

struct sc_nl_msg_t {
	unsigned int sc_cmd;
	unsigned int sc_subcmd;
	unsigned int sc_subcmd_para1;
	unsigned int sc_subcmd_para2;
	unsigned int sc_data_len;
	unsigned int sc_ret_data_len;
	unsigned int identity;
	char sc_data[SCD_NL_MSG_MAX_LEN];
};

struct scd_cmd_param_t_1 {
	int size;
	int data[50];
};

enum sc_info {
	SC_VBAT,
	SC_BAT_TMP,
	SC_UISOC,
	SC_SOC,
	SC_ENABLE,
	SC_BAT_SIZE,
	SC_START_TIME,
	SC_END_TIME,
	SC_IBAT_LIMIT,
	SC_TARGET_PERCENTAGE,
	SC_LEFT_TIME_FOR_CV,
	SC_IBAT_SETTING,
	SC_IBAT,
	SC_IBAT_ALG,
	SC_IBUS,
	SC_DBGLV,
	SC_SOLUTION,

	SC_INFO_MAX
};

enum sc_current_direction {
	SC_IGNORE,
	SC_KEEP,
	SC_DISABLE,
	SC_REDUCE,
};

struct smartcharging {
	/*daemon related*/
	struct sock *daemo_nl_sk;
	u_int g_scd_pid;
	struct scd_cmd_param_t_1 data;
	bool enable;
	int battery_size;
	int current_limit;
	int target_percentage;
	int left_time_for_cv;
	int start_time;
	int end_time;

	bool disable_charger;
	enum sc_current_direction solution;
	int sc_ibat;
	int pre_ibat;
	int bh;

	bool disable_in_this_plug;
};

extern int wakeup_sc_algo_cmd(struct scd_cmd_param_t_1 *data, int subcmd, int para1);
extern void sc_update(struct mtk_charger *info);
extern void sc_select_charging_current(struct mtk_charger *info, struct charger_data *pdata);

#endif /* End of _MTK_SMARTCHARGING_H */

