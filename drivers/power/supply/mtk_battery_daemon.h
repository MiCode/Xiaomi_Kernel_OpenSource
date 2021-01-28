/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#define NETLINK_FGD 26
#define MAX_NL_LEN_SND	4096
#define MAX_NL_LEN_RCV	9200
#define FGD_NL_LEN			sizeof(struct fgd_nl_msg_t)
#define FGD_NL_HDR_LEN		(FGD_NL_LEN - FGD_NL_MSG_MAX_LEN)
#define FGD_NL_MAGIC		2015060303
#define FGD_NL_MSG_MAX_LEN	9200
#define LOG_BUF_MAX			(MAX_NL_LEN_SND - FGD_NL_HDR_LEN - 1)

struct fgd_nl_msg_t {
	unsigned int nl_cmd;
	unsigned int fgd_cmd;
	unsigned int fgd_cmd_hash;
	unsigned int fgd_subcmd;
	unsigned int fgd_subcmd_para1;
	unsigned int fgd_data_len;
	unsigned int fgd_ret_data_len;
	unsigned int identity;
	char fgd_data[FGD_NL_MSG_MAX_LEN];
};

extern int mtk_battery_daemon_init(struct platform_device *pdev);
extern int wakeup_fg_daemon(unsigned int flow_state, int cmd, int para1);

#define DATA_SIZE 2048
struct fgd_cmd_param_t_4 {
	//unsigned int type;
	unsigned int total_size;
	unsigned int size;
	unsigned int idx;
	char input[DATA_SIZE];
};

