// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"
#include "mt-plat/mtk_thermal_platform.h"
#include <mtk_cooler_setting.h>
#if FEATURE_SPA
#include "mtk_ts_imgsensor.h"
#include "mtk_ts_pa.h"
#include "mtk_ts_wmt.h"
#include "mtk_cooler_atm.h"
#include "mtk_cooler_fps.h"
#include "mtk_cooler_bcct.h"
#include "mtk_cooler_bcct_v1.h"
#endif
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
#include <linux/uidgid.h>
#define MAX_LEN	128

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static unsigned int fg_app_pid;
static int mtkts_ta_debug_log;

#define tsta_dprintk(fmt, args...)   \
	do {                                    \
		if (mtkts_ta_debug_log) {                \
			pr_debug("[Thermal/TC/TA]" fmt, ##args); \
		}                                   \
	} while (0)

#define tsta_warn(fmt, args...)  pr_notice("[Thermal/TC/TA]" fmt, ##args)

/* ************************************ */
/* Weak functions */
/* ************************************ */
int __attribute__ ((weak))
get_image_sensor_state(void)
{
	return -1;
}

/*=============================================================
 *Local variable definition
 *=============================================================
 */

static struct sock *daemo_nl_sk;
static void ta_nl_send_to_user(
	int pid, int seq, struct tad_nl_msg_t *reply_msg);

static int g_tad_pid;
static bool init_flag;
static int g_tad_ttj;
struct SPA_T thermal_spa_t;
struct DCTM_T thermal_dctm_t;
static struct tad_nl_msg_t tad_ret_msg;
static unsigned int g_ta_status;
static int g_ta_counter;
/*=============================================================
 *Local function prototype
 *=============================================================
 */

/*=============================================================
 *Weak functions
 *=============================================================
 */
#define NETLINK_TAD 27
/*=============================================================*/






void atm_ctrl_cmd_from_user(void *nl_data, struct tad_nl_msg_t *ret_msg)
{
	struct tad_nl_msg_t *msg;

	msg = nl_data;

	/*tsta_dprintk(
	 *	"[atm_ctrl_cmd_from_user] tad_cmd = %d, tad_data_len = %d\n",
	 *msg->tad_cmd , msg->tad_data_len);
	 */

	ret_msg->tad_cmd = msg->tad_cmd;

	switch (msg->tad_cmd) {
	case TA_DAEMON_CMD_NOTIFY_DAEMON_CATMINIT:
		{
			memcpy(ret_msg->tad_data, &thermal_atm_t,
							sizeof(thermal_atm_t));

			ret_msg->tad_data_len += sizeof(thermal_atm_t);

			tsta_dprintk(
			"[%s] ret_msg->tad_data_len %d\n", __func__,
			ret_msg->tad_data_len);
		}
		break;
	case TA_DAEMON_CMD_GET_INIT_FLAG:
		{
			ret_msg->tad_data_len += sizeof(init_flag);
			memcpy(ret_msg->tad_data, &init_flag,
							sizeof(init_flag));

			tsta_dprintk(
				"[%s] init_flag = %d\n", __func__,
				init_flag);
		}
		break;
	case TA_DAEMON_CMD_GET_TPCB:
		{
			int curr_tpcb = mtk_thermal_get_temp(
							MTK_THERMAL_SENSOR_AP);

			ret_msg->tad_data_len += sizeof(curr_tpcb);
			memcpy(ret_msg->tad_data, &curr_tpcb,
						sizeof(curr_tpcb));

			tsta_dprintk(
				"[%s] curr_tpcb = %d\n", __func__,
								curr_tpcb);
		}
		break;

	case TA_DAEMON_CMD_GET_DCTM_DRCCFG:
		{
			memcpy(ret_msg->tad_data, &thermal_dctm_t,
							sizeof(thermal_dctm_t));

			ret_msg->tad_data_len += sizeof(thermal_dctm_t);

			tsta_dprintk(
			"[%s] ret_msg->tad_data_len %d\n", __func__,
			ret_msg->tad_data_len);
		}
		break;

	case TA_DAEMON_CMD_GET_DTCM:
		{
			int curr_tdctm = mtk_thermal_get_temp(
					MTK_THERMAL_SENSOR_DCTM);

			ret_msg->tad_data_len += sizeof(curr_tdctm);
			memcpy(ret_msg->tad_data, &curr_tdctm,
						sizeof(curr_tdctm));

			tsta_dprintk(
				"[%s] curr_tdctm = %d\n", __func__,
						curr_tdctm);
		}
		break;

	case TA_DAEMON_CMD_GET_TSCPU:
		{
			int curr_tscpu = mtk_thermal_get_temp(
							MTK_THERMAL_SENSOR_CPU);

			ret_msg->tad_data_len += sizeof(curr_tscpu);
			memcpy(ret_msg->tad_data, &curr_tscpu,
						sizeof(curr_tscpu));

			tsta_dprintk(
				"[%s] curr_tscpu = %d\n", __func__,
								curr_tscpu);
		}
		break;

	case TA_DAEMON_CMD_SET_DAEMON_PID:
		{
			memcpy(&g_tad_pid, &msg->tad_data[0],
						sizeof(g_tad_pid));

			tsta_dprintk(
				"[%s] g_tad_pid = %d\n", __func__,
								g_tad_pid);
		}
		break;
	case TA_DAEMON_CMD_SET_TTJ:
		{
			memcpy(&g_tad_ttj, &msg->tad_data[0],
						sizeof(g_tad_ttj));

			tsta_dprintk(
				"[%s] g_tad_ttj = %d\n", __func__,
								g_tad_ttj);
		}
		break;
	case TA_DAEMON_CMD_GET_TI:
		{
			/* --- SPA parameters --- */
#if FEATURE_SPA
			thermal_spa_t.t_spa_system_info.cpu_Tj =
				mtk_thermal_get_temp(MTK_THERMAL_SENSOR_CPU);

			thermal_spa_t.t_spa_system_info.Tpcb =
				mtk_thermal_get_temp(MTK_THERMAL_SENSOR_AP);

			thermal_spa_t.t_spa_system_info.OPP_power =
						clatm_get_curr_opp_power();

			thermal_spa_t.t_spa_system_info.fg_app_pid = fg_app_pid;

			thermal_spa_t.t_spa_system_info.avg_fps =
							clfps_get_disp_fps();

			thermal_spa_t.t_spa_system_info.WIFI_UL_Tput =
						tswmt_get_WiFi_tx_tput();

			thermal_spa_t.t_spa_system_info.MD_UL_Tput =
							tspa_get_MD_tx_tput();

			thermal_spa_t.t_spa_system_info.chg_current_limit =
						clbcct_get_chr_curr_limit();

			thermal_spa_t.t_spa_system_info.input_current_limit =
						clbcct_get_input_curr_limit();

			thermal_spa_t.t_spa_system_info.camera_on =
						get_image_sensor_state();

			thermal_spa_t.t_spa_system_info.game_mode =
							clfps_get_game_mode();
#else
			thermal_spa_t.t_spa_system_info.cpu_Tj = 0;
			thermal_spa_t.t_spa_system_info.Tpcb = 0;
			thermal_spa_t.t_spa_system_info.OPP_power = 0;
			thermal_spa_t.t_spa_system_info.fg_app_pid = 0;
			thermal_spa_t.t_spa_system_info.avg_fps = 0;
			thermal_spa_t.t_spa_system_info.WIFI_UL_Tput = 0;
			thermal_spa_t.t_spa_system_info.MD_UL_Tput = 0;
			thermal_spa_t.t_spa_system_info.chg_current_limit = 0;
			thermal_spa_t.t_spa_system_info.input_current_limit = 0;
			thermal_spa_t.t_spa_system_info.camera_on = 0;
			thermal_spa_t.t_spa_system_info.game_mode = 0;
#endif

			memcpy(ret_msg->tad_data, &thermal_spa_t,
							sizeof(thermal_spa_t));

			ret_msg->tad_data_len += sizeof(thermal_spa_t);

			tsta_dprintk(
			"[%s] ret_msg->tad_data_len %d\n", __func__,
				ret_msg->tad_data_len);
		}
		break;

	default:
		tsta_warn("bad TA_DAEMON_CTRL_CMD_FROM_USER 0x%x\n",
							msg->tad_cmd);
				g_ta_status = g_ta_status | 0x01000000;
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
	if (!skb) {
		g_ta_status = g_ta_status | 0x00010000;
		return;
	}
	nlh = nlmsg_put(skb, pid, seq, 0, size, 0);
	data = NLMSG_DATA(nlh);
	memcpy(data, reply_msg, size);
	NETLINK_CB(skb).portid = 0; /* from kernel */
	NETLINK_CB(skb).dst_group = 0; /* unicast */

	tsta_dprintk(
	"[%s] netlink_unicast size=%d tad_cmd=%d pid=%d\n", __func__,
		size, reply_msg->tad_cmd, pid);


	ret = netlink_unicast(daemo_nl_sk, skb, pid, MSG_DONTWAIT);
	if (ret < 0) {
		g_ta_status = g_ta_status | 0x00000010;
		pr_notice("[%s] send failed %d\n", __func__, ret);
		return;
	}


	tsta_dprintk("[%s] netlink_unicast- ret=%d\n", __func__, ret);

}


static void ta_nl_data_handler(struct sk_buff *skb)
{
	u32 pid;
	kuid_t uid;
	int seq;
	void *data;
	struct nlmsghdr *nlh;
	struct tad_nl_msg_t *tad_msg = NULL;
	int size = 0;

	nlh = (struct nlmsghdr *)skb->data;
	pid = NETLINK_CREDS(skb)->pid;
	uid = NETLINK_CREDS(skb)->uid;
	seq = nlh->nlmsg_seq;

	/*tsta_dprintk(
	 *"[ta_nl_data_handler] recv skb from user space uid:%d pid:%d seq:%d\n"
	 * ,uid, pid, seq);
	 */
	data = NLMSG_DATA(nlh);

	tad_msg = (struct tad_nl_msg_t *)data;
	if (tad_msg->tad_ret_data_len >= TAD_NL_MSG_MAX_LEN) {
		g_ta_status = g_ta_status | 0x00000100;
		tsta_warn("[%s] tad_msg->=ad_ret_data_len=%d\n", __func__,
		tad_msg->tad_ret_data_len);
		return;
	}

	size = tad_msg->tad_ret_data_len + TAD_NL_MSG_T_HDR_LEN;

	memset(&tad_ret_msg, 0, size);

	atm_ctrl_cmd_from_user(data, &tad_ret_msg);
	ta_nl_send_to_user(pid, seq, &tad_ret_msg);
	tsta_dprintk("[%s] send to user space process done\n", __func__);



}

int wakeup_ta_algo(int flow_state)
{
	tsta_dprintk("[%s]g_tad_pid=%d, state=%d\n", __func__, g_tad_pid,
								flow_state);

	/*Avoid print log too much*/
	if (g_ta_counter >= 3) {
		g_ta_counter = 0;
		if (g_ta_status != 0)
			tsta_warn("[%s] status: 0x%x\n", __func__, g_ta_status);
	}
	g_ta_counter++;
	if (g_tad_pid != 0) {
		struct tad_nl_msg_t *tad_msg = NULL;
		int size = TAD_NL_MSG_T_HDR_LEN + sizeof(flow_state);

		/*tad_msg = (struct tad_nl_msg_t *)vmalloc(size);*/
		tad_msg = kmalloc(size, GFP_KERNEL);

		if (tad_msg == NULL) {
			g_ta_status = g_ta_status | 0x00100000;
			return -ENOMEM;
		}
		tsta_dprintk("[%s] malloc size=%d\n", __func__, size);
		memset(tad_msg, 0, size);
		tad_msg->tad_cmd = TA_DAEMON_CMD_NOTIFY_DAEMON;
		memcpy(tad_msg->tad_data, &flow_state, sizeof(flow_state));
		tad_msg->tad_data_len += sizeof(flow_state);
		ta_nl_send_to_user(g_tad_pid, 0, tad_msg);
		kfree(tad_msg);
		return 0;
	}
	tsta_warn("[%s] error,g_tad_pid=0\n", __func__);
	g_ta_status = g_ta_status | 0x00001000;
	return -1;
}

static int tsta_read_log(struct seq_file *m, void *v)
{

	seq_printf(m, "[ %s] log = %d\n", __func__, mtkts_ta_debug_log);


	return 0;
}

static ssize_t tsta_write_log(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
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

	tsta_warn("%s bad argument\n", __func__);


	return -EINVAL;

}


static int tsta_open_log(struct inode *inode, struct file *file)
{
	return single_open(file, tsta_read_log, NULL);
}
static const struct proc_ops mtktsta_log_fops = {
	.proc_open = tsta_open_log,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_write = tsta_write_log,
	.proc_release = single_release,
};

static ssize_t clmutt_fg_pid_write(
struct file *filp, const char __user *buf, size_t count, loff_t *data)
{
	int ret = 0;
	char tmp[MAX_LEN] = {0};
	int len = 0;

	len = (count < (MAX_LEN - 1)) ? count : (MAX_LEN - 1);
	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	ret = kstrtouint(tmp, 10, &fg_app_pid);
	if (ret)
		WARN_ON(1);

	tsta_dprintk("[%s] %s = %d\n", __func__, tmp, fg_app_pid);

	return len;
}

static int clmutt_fg_pid_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", fg_app_pid);

	tsta_dprintk("[%s] %d\n", __func__, fg_app_pid);

	return 0;
}

static int clmutt_fg_pid_open(struct inode *inode, struct file *file)
{
	return single_open(file, clmutt_fg_pid_read, PDE_DATA(inode));
}

static const struct proc_ops clmutt_fg_pid_fops = {
	.proc_open = clmutt_fg_pid_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_write = clmutt_fg_pid_write,
	.proc_release = single_release,
};

static void tsta_create_fs(void)
{

	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktsta_dir = NULL;

	mtkts_ta_debug_log = 0;

	mtktsta_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktsta_dir) {
		tscpu_printk("[%s]: mkdir /proc/driver/thermal failed\n",
								__func__);
	} else {

		entry =
			proc_create("ta_log", 0644, mtktsta_dir,
						&mtktsta_log_fops);

		entry = proc_create("ta_fg_pid", 0664, mtktsta_dir,
						&clmutt_fg_pid_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}
}

int ta_init(void)
{
	/*add by willcai for the userspace  to kernelspace*/
	struct netlink_kernel_cfg cfg = {
		.input  = ta_nl_data_handler,
	};

	g_tad_pid = 0;
	init_flag = false;
	g_tad_ttj = 0;
	g_ta_status = 0;

	/*add by willcai for the userspace to kernelspace*/
	daemo_nl_sk = NULL;
	daemo_nl_sk = netlink_kernel_create(&init_net, NETLINK_TAD, &cfg);

	tsta_dprintk("netlink_kernel_create protol= %d\n", NETLINK_TAD);

	if (daemo_nl_sk == NULL) {
		tsta_warn("[%s] netlink_kernel_create error\n", __func__);
		g_ta_status = 0x00000001;
		return -1;
	}

	tsta_create_fs();

	tsta_dprintk("[%s] Initialization : DONE\n", __func__);

	return 0;

}





//module_init(ta_init);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");

