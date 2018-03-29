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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mt_thermal.h"
#include "mt-plat/mtk_thermal_platform.h"
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <tscpu_settings.h>
#include <mt-plat/aee.h>
#include <net/sock.h>
#include <net/genetlink.h>

#include <linux/netlink.h>
#include <linux/socket.h>
#include <linux/skbuff.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>

static int mtkts_ta_debug_log;

#define tsta_dprintk(fmt, args...)   \
	do {                                    \
		if (mtkts_ta_debug_log) {                \
			pr_debug("[Thermal_TA]" fmt, ##args); \
		}                                   \
	} while (0)

#define tsta_warn(fmt, args...)  pr_warn("[Thermal_TA]" fmt, ##args)

/*=============================================================
 *Local variable definition
 *=============================================================*/

static struct sock *daemo_nl_sk;
static void ta_nl_send_to_user(int pid, int seq, struct tad_nl_msg_t *reply_msg);
static int g_tad_pid;
static bool init_flag;
static int g_tad_ttj;


/*=============================================================
 *Local function prototype
 *=============================================================*/

/*=============================================================
 *Weak functions
 *=============================================================*/
#define NETLINK_TAD 27
/*=============================================================*/






void atm_ctrl_cmd_from_user(void *nl_data, struct tad_nl_msg_t *ret_msg)
{
	struct tad_nl_msg_t *msg;

	msg = nl_data;

	/*tsta_dprintk("[atm_ctrl_cmd_from_user] tad_cmd = %d, tad_data_len = %d\n" ,
	msg->tad_cmd , msg->tad_data_len);*/

	ret_msg->tad_cmd = msg->tad_cmd;

	switch (msg->tad_cmd) {
	case TA_DAEMON_CMD_NOTIFY_DAEMON_CATMINIT:
		{

			memcpy(ret_msg->tad_data, &thermal_atm_t, sizeof(thermal_atm_t));
			ret_msg->tad_data_len += sizeof(thermal_atm_t);

			tsta_dprintk("[atm_ctrl_cmd_from_user] ret_msg->tad_data_len %d\n" , ret_msg->tad_data_len);
		}
		break;
	case TA_DAEMON_CMD_GET_INIT_FLAG:
		{
			ret_msg->tad_data_len += sizeof(init_flag);
			memcpy(ret_msg->tad_data, &init_flag, sizeof(init_flag));
			tsta_dprintk("[atm_ctrl_cmd_from_user] init_flag = %d\n" , init_flag);
		}
		break;
	case TA_DAEMON_CMD_GET_TPCB:
		{
			int curr_tpcb = mtk_thermal_get_temp(MTK_THERMAL_SENSOR_AP);

			ret_msg->tad_data_len += sizeof(curr_tpcb);
			memcpy(ret_msg->tad_data, &curr_tpcb, sizeof(curr_tpcb));
			tsta_dprintk("[atm_ctrl_cmd_from_user] curr_tpcb = %d\n" , curr_tpcb);
		}
		break;

	case TA_DAEMON_CMD_SET_DAEMON_PID:
		{
			memcpy(&g_tad_pid, &msg->tad_data[0], sizeof(g_tad_pid));
			tsta_dprintk("[atm_ctrl_cmd_from_user] g_tad_pid = %d\n", g_tad_pid);
		}
		break;
	case TA_DAEMON_CMD_SET_TTJ:
		{
			memcpy(&g_tad_ttj, &msg->tad_data[0], sizeof(g_tad_ttj));
			tsta_dprintk("[atm_ctrl_cmd_from_user] g_tad_ttj = %d\n", g_tad_ttj);
		}
		break;

	default:
			tsta_warn("bad TA_DAEMON_CTRL_CMD_FROM_USER 0x%x\n", msg->tad_cmd);
		break;
	}

}

int ta_get_ttj(void)
{
	tsta_dprintk("g_tad_ttj= %d\n", g_tad_ttj);

	return g_tad_ttj;
}

static void ta_nl_send_to_user(int pid, int seq, struct tad_nl_msg_t *reply_msg)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int size = reply_msg->tad_data_len + TAD_NL_MSG_T_HDR_LEN;

	int len = NLMSG_SPACE(size);
	void *data;
	int ret;

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return;

	nlh = nlmsg_put(skb, pid, seq, 0, size, 0);
	data = NLMSG_DATA(nlh);
	memcpy(data, reply_msg, size);
	NETLINK_CB(skb).portid = 0; /* from kernel */
	NETLINK_CB(skb).dst_group = 0; /* unicast */

	tsta_dprintk("[ta_nl_send_to_user] netlink_unicast size=%d tad_cmd=%d pid=%d\n",
		size, reply_msg->tad_cmd, pid);


	ret = netlink_unicast(daemo_nl_sk, skb, pid, MSG_DONTWAIT);
	if (ret < 0)
		tsta_warn("[ta_nl_send_to_user] send failed %d\n", ret);


	tsta_dprintk("[ta_nl_send_to_user] netlink_unicast- ret=%d\n", ret);

}


static void ta_nl_data_handler(struct sk_buff *skb)
{
	u32 pid;
	kuid_t uid;
	int seq;
	void *data;
	struct nlmsghdr *nlh;
	struct tad_nl_msg_t *tad_msg, *tad_ret_msg;
	int size = 0;

	nlh = (struct nlmsghdr *)skb->data;
	pid = NETLINK_CREDS(skb)->pid;
	uid = NETLINK_CREDS(skb)->uid;
	seq = nlh->nlmsg_seq;

	/*tsta_dprintk("[ta_nl_data_handler] recv skb from user space uid:%d pid:%d seq:%d\n", uid, pid, seq);*/
	data = NLMSG_DATA(nlh);

	tad_msg = (struct tad_nl_msg_t *)data;

	size = tad_msg->tad_ret_data_len + TAD_NL_MSG_T_HDR_LEN;

	/*tad_ret_msg = (struct tad_nl_msg_t *)vmalloc(size);*/
	tad_ret_msg = vmalloc(size);
	memset(tad_ret_msg, 0, size);

	atm_ctrl_cmd_from_user(data, tad_ret_msg);
	ta_nl_send_to_user(pid, seq, tad_ret_msg);
	tsta_dprintk("[ta_nl_data_handler] send to user space process done\n");

	vfree(tad_ret_msg);
}

int wakeup_ta_algo(int flow_state)
{
	tsta_dprintk("[wakeup_ta_algo]g_tad_pid=%d, state=%d\n" , g_tad_pid, flow_state);

	if (g_tad_pid != 0) {
		struct tad_nl_msg_t *tad_msg;
		int size = TAD_NL_MSG_T_HDR_LEN + sizeof(flow_state);

		/*tad_msg = (struct tad_nl_msg_t *)vmalloc(size);*/
		tad_msg = vmalloc(size);
		tsta_dprintk("[wakeup_ta_algo] malloc size=%d\n", size);
		memset(tad_msg, 0, size);
		tad_msg->tad_cmd = TA_DAEMON_CMD_NOTIFY_DAEMON;
		memcpy(tad_msg->tad_data, &flow_state, sizeof(flow_state));
		tad_msg->tad_data_len += sizeof(flow_state);
		ta_nl_send_to_user(g_tad_pid, 0, tad_msg);
		vfree(tad_msg);
		return 0;
	} else {
		return -1;
	}
}

static int tsta_read_log(struct seq_file *m, void *v)
{

	seq_printf(m, "[ tsta_read_log] log = %d\n", mtkts_ta_debug_log);


	return 0;
}

static ssize_t tsta_write_log(struct file *file, const char __user *buffer, size_t count,
			       loff_t *data)
{
	char desc[32];
	int log_switch;
	int len = 0;


	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (kstrtoint(desc, 10, &log_switch) == 0) {
		mtkts_ta_debug_log = log_switch;

		return count;
	}

	tsta_warn("tscpu_write_log bad argument\n");


	return -EINVAL;

}


static int tsta_open_log(struct inode *inode, struct file *file)
{
	return single_open(file, tsta_read_log, NULL);
}
static const struct file_operations mtktsta_log_fops = {
	.owner = THIS_MODULE,
	.open = tsta_open_log,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tsta_write_log,
	.release = single_release,
};

static void tsta_create_fs(void)
{

	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktsta_dir = NULL;

	mtkts_ta_debug_log = 0;

	mtktsta_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktsta_dir) {
		tscpu_printk("[%s]: mkdir /proc/driver/thermal failed\n", __func__);
	} else {

		entry =
		    proc_create("ta_log", S_IRUGO | S_IWUSR, mtktsta_dir, &mtktsta_log_fops);
	}
}

static int __init ta_init(void)
{
	/*add by willcai for the userspace  to kernelspace*/
	struct netlink_kernel_cfg cfg = {
		.input  = ta_nl_data_handler,
	};

	g_tad_pid = 0;
	init_flag = false;
	g_tad_ttj = 0;

	/*add by willcai for the userspace to kernelspace*/
	daemo_nl_sk = NULL;
	daemo_nl_sk = netlink_kernel_create(&init_net, NETLINK_TAD, &cfg);

	tsta_dprintk("netlink_kernel_create protol= %d\n", NETLINK_TAD);

	if (daemo_nl_sk == NULL) {
		tsta_warn("[ta_init] netlink_kernel_create error\n");
		return -1;
	}

	tsta_create_fs();

	tsta_dprintk("[ta_init] Initialization : DONE\n");

	return 0;

}





module_init(ta_init);

