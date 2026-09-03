// SPDX-License-Identifier: GPL-2.0
//
// Unisoc ddr dvfs driver
//
// Copyright (C) 2022 Unisoc, Inc.
// Author: Mingmin Ling <mingmin.ling@unisoc.com>

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/panic_notifier.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/sipc.h>
#include "sprd_ddr_dvfs.h"
#include <../drivers/unisoc_platform/sysdump/unisoc_sysdump.h>

enum dvfs_master_cmd {
	DVFS_CMD_NORMAL			= 0x0000,
	DVFS_CMD_ENABLE			= 0x0300,
	DVFS_CMD_DISABLE		= 0x0305,
	DVFS_CMD_AUTO_ENABLE		= 0x0310,
	DVFS_CMD_AUTO_DISABLE		= 0x0315,
	DVFS_CMD_PERF_MODE_ENABLE	= 0x031A,
	DVFS_CMD_PERF_MODE_DISABLE	= 0x031F,
	DVFS_CMD_AXI_ENABLE		= 0x0320,
	DVFS_CMD_AXI_DISABLE		= 0x0330,
	DVFS_CMD_INQ_DDR_FREQ		= 0x0500,
	DVFS_CMD_INQ_AP_FREQ		= 0x0502,
	DVFS_CMD_INQ_CP_FREQ		= 0x0503,
	DVFS_CMD_INQ_DDR_TABLE		= 0x0505,
	DVFS_CMD_INQ_COUNT		= 0x0507,
	DVFS_CMD_INQ_STATUS		= 0x050A,
	DVFS_CMD_INQ_AUTO_STATUS	= 0x050B,
	DVFS_CMD_INQ_OVERFLOW		= 0x0510,
	DVFS_CMD_INQ_UNDERFLOW		= 0x0520,
	DVFS_CMD_INQ_TIMER		= 0x0530,
	DVFS_CMD_INQ_AXI		= 0x0540,
	DVFS_CMD_INQ_AXI_WLTC		= 0x0541,
	DVFS_CMD_INQ_AXI_RLTC		= 0x0542,
	DVFS_CMD_SET_DDR_FREQ		= 0x0600,
	DVFS_CMD_SET_CAL_FREQ		= 0x0603,
	DVFS_CMD_PARA_START		= 0x0700,
	DVFS_CMD_PARA_OVERFLOW		= 0x0710,
	DVFS_CMD_PARA_UNDERFLOW		= 0x0720,
	DVFS_CMD_PARA_TIMER		= 0x0730,
	DVFS_CMD_PARA_END		= 0x07FF,
	DVFS_CMD_SET_AXI_WLTC		= 0x0810,
	DVFS_CMD_SET_AXI_RLTC		= 0x0820,
	DVFS_CMD_DEBUG			= 0x0FFF
};

enum dvfs_slave_cmd {
	DVFS_RET_ADJ_OK			= 0x0000,
	DVFS_RET_ADJ_VER_FAIL		= 0x0001,
	DVFS_RET_ADJ_BUSY		= 0x0002,
	DVFS_RET_ADJ_NOCHANGE		= 0x0003,
	DVFS_RET_ADJ_FAIL		= 0x0004,
	DVFS_RET_DISABLE		= 0x0005,
	DVFS_RET_ON_OFF_SUCCEED		= 0x0300,
	DVFS_RET_ON_OFF_FAIL		= 0x0303,
	DVFS_RET_INQ_SUCCEED		= 0x0500,
	DVFS_RET_INQ_FAIL		= 0x0503,
	DVFS_RET_SET_SUCCEED		= 0x0600,
	DVFS_RET_SET_FAIL		= 0x0603,
	DVFS_RET_PARA_OK		= 0x070F,
	DVFS_RET_DEBUG_OK		= 0x0F00,
	DVFS_RET_INVALID_CMD		= 0x0F0F
};

struct freq_para {
	unsigned int overflow;
	unsigned int underflow;
	unsigned int vol;
};

struct dvfs_data {
	struct device *dev;
	struct devfreq *devfreq;
	struct devfreq_dev_profile *profile;
	struct task_struct *dvfs_smsg_ch_open;
	struct mutex sync_mutex;
	unsigned int freq_num;
	unsigned long *freq_table;
	unsigned long *freq_table_display;
	struct freq_para *paras;
	unsigned int force_freq;
	unsigned int request_freq;
	struct dvfs_hw_callback *hw_callback;
	struct completion reg_callback_done;
	struct governor_callback *gov_callback;
	unsigned int init_done;
	struct delayed_work topfreq_unvote_work;
	unsigned int dvfs_smsg_thread_process;
	struct wakeup_source *wake_lock;
};
static struct dvfs_data *g_dvfs_data;
static char *default_governor = "sprd-governor";

struct ddr_dfs_step_list_t *ddr_cur_step_g;
struct ddr_dfs_step_list_t ddr_step_arr[DDR_DB_NODE_NUM] = {0};
static char *g_ddr_dvfs_dump;

static struct ddr_dfs_step_list_t *ddr_step_list_init(u32 node_num)
{
	u32 i;
	struct ddr_dfs_step_list_t *head = ddr_step_arr;
	struct ddr_dfs_step_list_t *p = head;

	for (i = 1; i < node_num; i++) {
		snprintf(p->data.comm, COMM_MAX, "NONE");
		p->next = ddr_step_arr + i;
		p = p->next;
	}

	snprintf(p->data.comm, COMM_MAX, "NONE");
	p->next = head;

	return p;
}

static void ddr_dfs_step_add(enum DDR_DFS_STATE_STEP cur_step, int status, char *scene,
			     u32 buff, int pid, char *comm, ktime_t time)
{
	ddr_cur_step_g = ddr_cur_step_g->next;

	ddr_cur_step_g->data.step = cur_step;
	ddr_cur_step_g->data.status = status;

	memset(ddr_cur_step_g->data.scene, 0, SCENE_MAX);
	memset(ddr_cur_step_g->data.comm, 0, COMM_MAX);
	if (scene != NULL) {
		if (buff >= SCENE_MAX)
			memcpy(ddr_cur_step_g->data.scene, scene, SCENE_MAX - 1);
		else
			memcpy(ddr_cur_step_g->data.scene, scene, buff);
	}
	snprintf(ddr_cur_step_g->data.comm, COMM_MAX, "%s", comm);
	ddr_cur_step_g->data.buff = buff;
	ddr_cur_step_g->data.pid = pid;
	ddr_cur_step_g->data.time = time;
}

static int ddrinfo_dfs_step_parse(char **arg, char **step_status, char **scene,
				  u32 *buff, int *pid, char **comm, ktime_t *time, u32 i)
{
	int ret = 0;

	ddr_cur_step_g = ddr_cur_step_g->next;
	*scene = NULL;

	switch (ddr_cur_step_g->data.step) {
	case SCENARIO_DFS_ENTER:
		*arg = "scenario_dfs_enter";
		*scene = ddr_cur_step_g->data.scene;
		break;
	case EXIT_SCENE:
		*arg = "exit_scene";
		*scene = ddr_cur_step_g->data.scene;
		break;
	case AUTO_DFS_ON_OFF:
		*arg = "auto_dfs_on_off";
		break;
	case SCALING_FORCE_DDR_FREQ:
		*arg = "scaling_force_ddr_freq";
		break;
	case SCENE_BOOST_ENTER:
		*arg = "scene_boost_enter";
		break;
	case SET_BACKDOOR:
		*arg = "set_backdoor";
		break;
	case DFS_ON_OFF:
		*arg = "dfs_on_off";
		break;
	case CHANGE_POINT:
		*arg = "change_point";
		break;
	case SCENE_FREQ_SET:
		*arg = "scene_freq_set";
		break;
	case GET_OVERFLOW_T:
		*arg = "get_overflow_t";
		break;
	case SET_OVERFLOW_T:
		*arg = "set_overflow_t";
		*scene = ddr_cur_step_g->data.scene;
		break;
	case GET_UNDERFLOW_T:
		*arg = "get_underflow_t";
		break;
	case SET_UNDERFLOW_T:
		*arg = "set_underflow_t";
		*scene = ddr_cur_step_g->data.scene;
		break;
	case GET_DVFS_STATUS_T:
		*arg = "get_dvfs_status_t";
		break;
	case GET_DVFS_AUTO_STATUS_T:
		*arg = "get_dvfs_auto_status_t";
		break;
	case GET_DVFS_FORCE_FREQ_T:
		*arg = "get_dvfs_force_freq_t";
		break;
	case GET_CUR_FREQ_T:
		*arg = "get_cur_freq_t";
		break;
	case GET_FREQ_TABLE_T:
		*arg = "get_freq_table_t";
		break;
	case SEND_FREQ_REQUEST_T:
		*arg = "send_freq_request_t";
		break;
	default:
		*arg = "NONE_STEP";
		break;
	}
	if (ddr_cur_step_g->data.status == 0)
		*step_status = "pass";
	else
		*step_status = "fail";
	*buff = ddr_cur_step_g->data.buff;
	*comm = ddr_cur_step_g->data.comm;
	*pid = ddr_cur_step_g->data.pid;
	*time = ddr_cur_step_g->data.time;
	if (i >= (DDR_DB_NODE_NUM - 1))
		ret =  1;

	return ret;
}

static int ddr_dvfs_panic_handler(struct notifier_block *self, unsigned long val, void *reason)
{
	ssize_t count = 0;
	unsigned int i = 0;
	char *arg = "NONE_STEP", *step_status = "NONE_STATUS";
	char *scene = NULL, *comm = NULL;
	int err, pid = -1, buff = 0;
	ktime_t time = 0;

	if (g_dvfs_data == NULL)
		return NOTIFY_DONE;

	if (g_dvfs_data->init_done != DDR_DVFS_INIT_DONE) {
		dev_info(g_dvfs_data->dev, "ddr dvfs driver not ready, no need dump dvfs step\n");
		return NOTIFY_DONE;
	}

	if (g_ddr_dvfs_dump) {
		do {
			err = ddrinfo_dfs_step_parse(&arg, &step_status, &scene,
						     &buff, &pid, &comm, &time, i);

			if (scene == NULL)
				count += snprintf(&g_ddr_dvfs_dump[count], INFO_LEN_MAX,
						  "DDR_DFS_STEP: %s %s, buff: %u, pid: %d, comm: %s, time: %lld ms\n",
						  arg, step_status, buff, pid, comm, time);
			else
				count += snprintf(&g_ddr_dvfs_dump[count], INFO_LEN_MAX,
						  "DDR_DFS_STEP: %s %s, scene: %s, pid: %d, comm: %s, time: %lld ms\n",
						  arg, step_status, scene, pid, comm, time);
			i++;
		} while (!err);
	}

	return NOTIFY_DONE;
}

static struct notifier_block ddr_dvfs_event_nb = {
	.notifier_call  = ddr_dvfs_panic_handler,
	.priority       = INT_MAX,
};

static void ddrinfo_dfs_step_debug_init(void)
{
	g_ddr_dvfs_dump = devm_kzalloc(g_dvfs_data->dev, DDR_DUMP_BUFFER, GFP_KERNEL);
	if (g_ddr_dvfs_dump) {
		atomic_notifier_chain_register(&panic_notifier_list, &ddr_dvfs_event_nb);
		if (minidump_save_extend_information("ddr_dvfs_history", __pa(g_ddr_dvfs_dump),
						     __pa(g_ddr_dvfs_dump + DDR_DUMP_BUFFER)))
			dev_err(g_dvfs_data->dev, "fail to link ddr_dvfs_history to minidump\n");
	}
}

static int dvfs_msg_recv(struct smsg *msg, int timeout)
{
	int err;
	struct device *dev = g_dvfs_data->dev;

	err = smsg_recv(SIPC_ID_PM_SYS, msg, timeout);
	if (err < 0) {
		dev_err(dev, "dvfs receive failed:%d\n", err);
		return err;
	}
	if (msg->channel == SMSG_CH_PM_CTRL &&
	    msg->type == SMSG_TYPE_DFS_RSP)
		return 0;

	return -EINVAL;
}

static int dvfs_msg_send(struct smsg *msg, unsigned int cmd, int timeout,
			 unsigned int value)
{
	int err;
	struct device *dev = g_dvfs_data->dev;

	smsg_set(msg, SMSG_CH_PM_CTRL, SMSG_TYPE_DFS, cmd, value);
	err = smsg_send(SIPC_ID_PM_SYS, msg, timeout);
	dev_dbg(dev, "send: channel=%d, type=%d, flag=0x%x, value=0x%x\n",
		msg->channel, msg->type, msg->flag, msg->value);
	if (err < 0) {
		dev_err(dev, "dvfs send failed, freq:%d, cmd:%d\n",
			value, cmd);
		return err;
	}

	return 0;
}

static int dvfs_msg_parse_ret(struct smsg *msg)
{
	unsigned int err;
	struct device *dev = g_dvfs_data->dev;

	switch (msg->flag) {
	case DVFS_RET_ADJ_OK:
	case DVFS_RET_ON_OFF_SUCCEED:
	case DVFS_RET_INQ_SUCCEED:
	case DVFS_RET_SET_SUCCEED:
	case DVFS_RET_PARA_OK:
	case DVFS_RET_DEBUG_OK:
		err = 0;
		break;
	case DVFS_RET_ADJ_VER_FAIL:
		dev_info(dev, "dvfs verify fail!current freq:%d\n",
			 msg->value);
		err = -EIO;
		break;
	case DVFS_RET_ADJ_BUSY:
		dev_info(dev, "dvfs busy!current freq:%d\n",
			 msg->value);
		err = -EBUSY;
		break;
	case DVFS_RET_ADJ_NOCHANGE:
		dev_info(dev, "dvfs no change!current freq:%d\n",
			 msg->value);
		err = -EFAULT;
		break;
	case DVFS_RET_ADJ_FAIL:
		dev_info(dev, "dvfs fail!current freq:%d\n",
			 msg->value);
		err = -EFAULT;
		break;
	case DVFS_RET_DISABLE:
		dev_info(dev, "dvfs is disabled!current freq:%d\n",
			 msg->value);
		err = -EPERM;
		break;
	case DVFS_RET_ON_OFF_FAIL:
		dev_err(dev, "dvfs enable verify failed\n");
		err = -EINVAL;
		break;
	case DVFS_RET_INQ_FAIL:
		dev_err(dev, "dvfs inquire failed\n");
		err = -EINVAL;
		break;
	case DVFS_RET_SET_FAIL:
		dev_err(dev, "dvfs set failed\n");
		err = -EINVAL;
		break;
	case DVFS_RET_INVALID_CMD:
		dev_info(dev, "no this command\n");
		err = -EINVAL;
		break;
	default:
		dev_info(dev, "dvfs invalid cmd:%x!current freq:%d\n",
			 msg->flag, msg->value);
		err = -EINVAL;
		break;
	}

	return err;
}

static int dvfs_msg(unsigned int *data, unsigned int value,
		    unsigned int cmd, unsigned int wait)
{
	int err;
	struct smsg msg;

	__pm_stay_awake(g_dvfs_data->wake_lock);
	err = dvfs_msg_send(&msg, cmd, msecs_to_jiffies(100), value);
	if (err < 0)
		goto ret;

	err = dvfs_msg_recv(&msg, msecs_to_jiffies(wait));
	if (err < 0)
		goto ret;

	err = dvfs_msg_parse_ret(&msg);
	*data = msg.value;
ret:
	__pm_relax(g_dvfs_data->wake_lock);

	return err;
}

static int dvfs_enable(void)
{
	int err;
	unsigned int data;

	err = dvfs_msg(&data, 0, DVFS_CMD_ENABLE, 2000);

	return err;
}

static int dvfs_disable(void)
{
	int err;
	unsigned int data;

	err = dvfs_msg(&data, 0, DVFS_CMD_DISABLE, 2000);

	return err;
}

static int dvfs_auto_enable(void)
{
	int err;
	unsigned int data;

	err = dvfs_msg(&data, 0, DVFS_CMD_AUTO_ENABLE, 2000);

	return err;
}

static int dvfs_auto_disable(void)
{
	int err;
	unsigned int data;

	err = dvfs_msg(&data, 0, DVFS_CMD_AUTO_DISABLE, 2000);

	return err;
}

static int get_dvfs_status(unsigned int *data)
{
	int err;

	if (g_dvfs_data == NULL)
		return -EINVAL;

	err =  dvfs_msg(data, 0, DVFS_CMD_INQ_STATUS, 500);

	return err;
}

static int get_dvfs_auto_status(unsigned int *data)
{
	int err;

	if (g_dvfs_data == NULL)
		return -EINVAL;

	err = dvfs_msg(data, 0, DVFS_CMD_INQ_AUTO_STATUS, 500);

	return err;
}

static int force_freq_request(unsigned int freq)
{
	unsigned int data;
	int err;

	if ((g_dvfs_data == NULL) || (g_dvfs_data->init_done != DDR_DVFS_INIT_DONE))
		return -EINVAL;
	if (unlikely(freq == 0xbacd))
		freq = get_max_freq();

	mutex_lock(&g_dvfs_data->sync_mutex);
	err = dvfs_msg(&data, freq, DVFS_CMD_SET_DDR_FREQ, 500);
	if (err == 0 && data == freq)
		g_dvfs_data->force_freq = freq;
	mutex_unlock(&g_dvfs_data->sync_mutex);

	return err;
}

unsigned long get_min_freq(void)
{
	return g_dvfs_data->devfreq->scaling_min_freq;
}

unsigned long get_max_freq(void)
{
	return g_dvfs_data->devfreq->scaling_max_freq;
}

static int send_freq_request(unsigned int freq)
{
	int i, err;
	unsigned long data;

	if ((g_dvfs_data == NULL) || (g_dvfs_data->init_done != DDR_DVFS_INIT_DONE))
		return -EINVAL;

	for (i = g_dvfs_data->freq_num - 1; i >= 0; i--) {
		err = g_dvfs_data->gov_callback->get_freq_table(&data, i);
		if (!err && data > 0) {
			if (freq == data)
				break;
		}
	}
	if (i == -1 && freq != 0)
		return -EINVAL;

	err = g_dvfs_data->hw_callback->dvfs_freq_request(freq);
	if (err == 0) {
		mutex_lock(&g_dvfs_data->sync_mutex);
		g_dvfs_data->request_freq = freq;
		mutex_unlock(&g_dvfs_data->sync_mutex);
	}

	return err;
}

static int get_force_freq(unsigned int *data)
{
	if ((g_dvfs_data == NULL) || (g_dvfs_data->init_done != DDR_DVFS_INIT_DONE))
		return -EINVAL;

	*data = g_dvfs_data->force_freq;

	return 0;
}

static int get_request_freq(unsigned int *data)
{
	if ((g_dvfs_data == NULL) || (g_dvfs_data->init_done != DDR_DVFS_INIT_DONE))
		return -EINVAL;

	*data = g_dvfs_data->request_freq;

	return 0;
}

int send_vote_request(unsigned int freq)
{
	int err;
	unsigned int data;

	if ((g_dvfs_data == NULL) || (g_dvfs_data->init_done != DDR_DVFS_INIT_DONE))
		return -EINVAL;

	mutex_lock(&g_dvfs_data->sync_mutex);
	err = dvfs_msg(&data, freq, DVFS_CMD_NORMAL, 500);
	mutex_unlock(&g_dvfs_data->sync_mutex);

	return err;
}

static int get_freq_table(unsigned long *data, unsigned int sel)
{
	int err;
	unsigned int freq_data = 0;

	if (g_dvfs_data == NULL)
		return -EINVAL;

	if (g_dvfs_data->init_done != DDR_DVFS_INIT_DONE) {
		err = dvfs_msg(&freq_data, sel, DVFS_CMD_INQ_DDR_TABLE, 2000);
		*data = (unsigned long)freq_data;
	} else {
		*data = g_dvfs_data->freq_table[sel];
		err = 0;
	}

	return err;
}

static int get_cur_freq(unsigned int *data)
{
	int err;

	if (g_dvfs_data == NULL)
		return -EINVAL;

	err = dvfs_msg(data, 0, DVFS_CMD_INQ_DDR_FREQ, 500);

	return err;
}

static int get_overflow(unsigned int *data, unsigned int sel)
{
	int err;

	if (g_dvfs_data == NULL)
		return -EINVAL;

	if (g_dvfs_data->init_done != DDR_DVFS_INIT_DONE) {
		err = dvfs_msg(data, sel, DVFS_CMD_INQ_OVERFLOW, 500);
	} else {
		mutex_lock(&g_dvfs_data->sync_mutex);
		*data = g_dvfs_data->paras[sel].overflow;
		mutex_unlock(&g_dvfs_data->sync_mutex);
		err = 0;
	}

	return err;
}

static int get_underflow(unsigned int *data, unsigned int sel)
{
	int err;

	if (g_dvfs_data == NULL)
		return -EINVAL;

	if (g_dvfs_data->init_done != DDR_DVFS_INIT_DONE) {
		err = dvfs_msg(data, sel, DVFS_CMD_INQ_UNDERFLOW, 500);
	} else {
		mutex_lock(&g_dvfs_data->sync_mutex);
		*data = g_dvfs_data->paras[sel].underflow;
		mutex_unlock(&g_dvfs_data->sync_mutex);
		err = 0;
	}

	return err;
}

static int set_overflow(unsigned int value, unsigned int sel)
{
	int err;
	unsigned int data;

	if (g_dvfs_data == NULL || sel >= g_dvfs_data->freq_num)
		return -EINVAL;

	mutex_lock(&g_dvfs_data->sync_mutex);
	err = dvfs_msg(&data, value, DVFS_CMD_PARA_OVERFLOW+sel, 500);
	if ((err == 0) && (g_dvfs_data->init_done == DDR_DVFS_INIT_DONE))
		g_dvfs_data->paras[sel].overflow = value;
	mutex_unlock(&g_dvfs_data->sync_mutex);

	return err;
}

static int set_underflow(unsigned int value, unsigned int sel)
{
	int err;
	unsigned int data;

	if (g_dvfs_data == NULL || sel >= g_dvfs_data->freq_num)
		return -EINVAL;

	mutex_lock(&g_dvfs_data->sync_mutex);
	err = dvfs_msg(&data, value, DVFS_CMD_PARA_UNDERFLOW+sel, 500);
	if ((err == 0) && (g_dvfs_data->init_done == DDR_DVFS_INIT_DONE))
		g_dvfs_data->paras[sel].underflow = value;
	mutex_unlock(&g_dvfs_data->sync_mutex);

	return err;
}

static int dvfs_perf_mode_enable(bool en)
{
	int err;
	unsigned int data;

	if (en == 1)
		err = dvfs_msg(&data, 0, DVFS_CMD_PERF_MODE_ENABLE, 3000);
	else
		err = dvfs_msg(&data, 0, DVFS_CMD_PERF_MODE_DISABLE, 3000);

	return err;
}

static int get_freq_num(unsigned int *data)
{
	if (g_dvfs_data == NULL)
		return -EINVAL;

	*data = g_dvfs_data->freq_num;

	return 0;
}

static int gov_vote(const char *name)
{
	if ((g_dvfs_data == NULL) || (g_dvfs_data->init_done != DDR_DVFS_INIT_DONE))
		return -EINVAL;

	return g_dvfs_data->hw_callback->hw_dvfs_vote(name);
}

static int gov_unvote(const char *name)
{
	if ((g_dvfs_data == NULL) || (g_dvfs_data->init_done != DDR_DVFS_INIT_DONE))
		return -EINVAL;

	return g_dvfs_data->hw_callback->hw_dvfs_unvote(name);
}

static int gov_change_point(const char *name, unsigned int freq)
{
	if ((g_dvfs_data == NULL) || (g_dvfs_data->init_done != DDR_DVFS_INIT_DONE))
		return -EINVAL;

	return g_dvfs_data->hw_callback->hw_dvfs_set_point(name, freq);
}

static int get_point_info(char **name, unsigned int *freq,
			unsigned int *flag, int index)
{
	if (g_dvfs_data == NULL)
		return -EINVAL;

	return g_dvfs_data->hw_callback->hw_dvfs_get_point_info(name, freq, flag, index);
}

struct governor_callback g_gov_callback = {
	.governor_vote = gov_vote,
	.governor_unvote = gov_unvote,
	.governor_change_point = gov_change_point,
	.get_point_info = get_point_info,
	.get_freq_num = get_freq_num,
	.get_overflow = get_overflow,
	.set_overflow = set_overflow,
	.get_underflow = get_underflow,
	.set_underflow = set_underflow,
	.get_dvfs_status = get_dvfs_status,
	.dvfs_enable = dvfs_enable,
	.dvfs_disable = dvfs_disable,
	.get_dvfs_auto_status = get_dvfs_auto_status,
	.dvfs_auto_enable = dvfs_auto_enable,
	.dvfs_auto_disable = dvfs_auto_disable,
	.dvfs_perf_mode_enable = dvfs_perf_mode_enable,
	.get_cur_freq = get_cur_freq,
	.get_freq_table = get_freq_table,
	.ddrinfo_dfs_step_parse = ddrinfo_dfs_step_parse,
	.ddr_dfs_step_add = ddr_dfs_step_add,
	.get_request_freq = get_request_freq,
	.send_freq_request = send_freq_request,
	.get_force_freq = get_force_freq,
};

static int dvfs_freq_target(struct device *dev, unsigned long *freq, u32 flags)
{
	int err;

	err = force_freq_request(*freq);
	if (err < 0)
		dev_err(dev, "set freq fail: %d\n", err);

	return err;
}

static int dvfs_get_dev_status(struct device *dev, struct devfreq_dev_status *state)
{
	int err;

	err = get_cur_freq((unsigned int *)&state->current_frequency);
	if (err < 0)
		dev_err(dev, "get cur freq fail: %d\n", err);
	state->private_data = (void *)g_dvfs_data->gov_callback;

	return err;
}

static int dvfs_get_cur_freq(struct device *dev, unsigned long *freq)
{
	int err;

	err = get_cur_freq((unsigned int *)freq);
	if (err < 0)
		dev_err(dev, "get cur freq fail: %d\n", err);

	return err;
}

static void dvfs_exit(struct device *dev)
{
	int err;

	err = dvfs_disable();
	if (err < 0)
		dev_err(dev, "disable fail: %d\n", err);
}

static void topfreq_unvote_work_handler(struct work_struct *work)
{
	struct device *dev = g_dvfs_data->dev;

	scene_exit("boot-opt");
	dev_info(dev, "dfs_init boot-opt scene cancel\n");
#ifdef CONFIG_SPRD_DDR_DVFS_GLP
	gov_vote("lcdoff");
	gov_unvote("lcdoff");
#endif
}

static void set_profile(struct devfreq_dev_profile *profile)
{
	profile->polling_ms = 0;
	profile->freq_table = (unsigned long *)g_dvfs_data->freq_table_display;
	profile->target = dvfs_freq_target;
	profile->get_dev_status = dvfs_get_dev_status;
	profile->get_cur_freq = dvfs_get_cur_freq;
	profile->exit = dvfs_exit;
}

static int ddr_freq_overflow_set(struct dvfs_data *data, u32 fn)
{
	int err;
	struct device *dev = data->dev;

	if (data->paras[fn].overflow != PARSE_FLOW_ERR) {
		err = set_overflow(data->paras[fn].overflow, fn);
		if (err < 0)
			dev_err(dev, "failed to set overflow %d\n", data->paras[fn].overflow);
	} else {
		err = get_overflow(&data->paras[fn].overflow, fn);
		if (err < 0)
			dev_err(dev, "failed to get overflow index: %d\n", fn);
	}

	return err;
}

static int ddr_freq_underflow_set(struct dvfs_data *data, u32 fn)
{
	int err;
	struct device *dev = data->dev;

	if (data->paras[fn].underflow != PARSE_FLOW_ERR) {
		err = set_underflow(data->paras[fn].underflow, fn);
		if (err < 0)
			dev_err(dev, "failed to set underflow %d\n", data->paras[fn].underflow);
	} else {
		err = get_underflow(&data->paras[fn].underflow, fn);
		if (err < 0)
			dev_err(dev, "get_underflow err\n");
	}

	return err;
}

static int dvfs_smsg_thread(void *value)
{
	struct dvfs_data *data = (struct dvfs_data *)value;
	struct device *dev = data->dev;
	char *temp_name;
	int i, err, effective_freq_count = 0;

	data->dvfs_smsg_thread_process = 1;
	while (smsg_ch_open(SIPC_ID_PM_SYS, SMSG_CH_PM_CTRL, -1)) {
		if (kthread_should_stop())
			return 0;
		msleep(500);
	}

	dev_err(dev, "smsg_ch_open finish!\n");
	while (dvfs_enable()) {
		if (kthread_should_stop())
			return 0;
		msleep(500);
	}

	for (i = 0; i < data->freq_num; i++) {
		err = get_freq_table(&data->freq_table[i], i);
		if (data->freq_table[i] != 0 && data->freq_table[i] != 0xff)
			data->freq_table_display[effective_freq_count++] = data->freq_table[i];
		if (err < 0) {
			dev_err(dev, "failed to get frequency index: %d\n", i);
			goto remove_thread;
		}

		err = ddr_freq_overflow_set(data, i);
		if (err < 0)
			goto remove_thread;

		err = ddr_freq_underflow_set(data, i);
		if (err < 0)
			goto remove_thread;

		/*fix me : now we do not have interface for vol*/
		if (data->paras[i].vol == 0)
			data->paras[i].vol = DEFAULT_VOL;
		if (data->freq_table[i] != 0 && data->freq_table[i] != 0xff) {
			err = dev_pm_opp_add(dev, data->freq_table[i], data->paras[i].vol);
			if (err < 0) {
				dev_err(dev, "failed to add opp: %luMHZ-%uuv\n",
					data->freq_table[i], data->paras[i].vol);
				goto remove_thread;
			}
		}
	}

	data->profile->max_state = effective_freq_count;

	err = get_cur_freq((unsigned int *)(&data->profile->initial_freq));
	if (err < 0) {
		dev_err(dev, "failed to get initial freq\n");
		goto remove_thread;
	}

	err = sprd_dvfs_add_governor();
	if (err < 0) {
		dev_err(dev, "failed to add governor\n");
		goto remove_thread;
	}
	err = of_property_read_string(dev->of_node, "governor", (const char **)&temp_name);
	if (err != 0) {
		dev_warn(dev, "no governor sepecific\n");
		temp_name = default_governor;
	}

	data->devfreq = devfreq_add_device(dev, data->profile, temp_name, NULL);
	if (IS_ERR(data->devfreq)) {
		dev_err(dev, "add freq devices fail\n");
		goto remove_governor;
	}

	err = dvfs_auto_enable();
	if (err < 0) {
		dev_err(dev, "dvfs auto enable failed\n");
		goto remove_device;
	}
	wait_for_completion(&data->reg_callback_done);
	data->init_done = DDR_DVFS_INIT_DONE;
	dev_info(dev, "ddr dvfs driver probe ok\n");

	scene_dfs_request("boot-opt");
	dev_info(dev, "dfs_init scene set boot-opt\n");
	//delay 30s to cancel topfreq vote
	schedule_delayed_work(&g_dvfs_data->topfreq_unvote_work, msecs_to_jiffies(30000));
	data->dvfs_smsg_thread_process = 0;

	return 0;

remove_device:
	devfreq_remove_device(data->devfreq);
remove_governor:
	sprd_dvfs_del_governor();
remove_thread:
	data->dvfs_smsg_thread_process = 0;

	return 0;
}

static struct dvfs_data *dvfs_mem_alloc(struct device *dev, unsigned int freq_num)
{
	struct dvfs_data *driver_data;

	driver_data = devm_kzalloc(dev, sizeof(struct dvfs_data), GFP_KERNEL);
	if (driver_data == NULL)
		goto err;

	driver_data->profile = devm_kzalloc(dev, sizeof(struct devfreq_dev_profile), GFP_KERNEL);
	if (driver_data->profile == NULL)
		goto err;

	driver_data->freq_table_display = devm_kzalloc(dev, (sizeof(unsigned long)) * freq_num,
						       GFP_KERNEL);
	if (driver_data->freq_table_display == NULL)
		goto err;


	driver_data->freq_table = devm_kzalloc(dev, (sizeof(unsigned long)) * freq_num, GFP_KERNEL);
	if (driver_data->freq_table == NULL)
		goto err;

	driver_data->paras = devm_kzalloc(dev, sizeof(struct freq_para)*freq_num, GFP_KERNEL);
	if (driver_data->paras == NULL)
		goto err;

	return driver_data;

err:
	return NULL;
}

int dvfs_core_init(struct platform_device *pdev)
{
	unsigned int freq_num, i;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	int err, err_uf = 0;

	if (g_dvfs_data != NULL) {
		dev_err(dev, "dvfs core can used by single device only\n");
		return -EINVAL;
	}

	err = of_property_read_u32(dev->of_node, "freq-num", &freq_num);
	if (err != 0) {
		dev_warn(dev, "could not detect freq-num, use default 8\n");
		freq_num = 8;
	}

	g_dvfs_data = dvfs_mem_alloc(dev, freq_num);
	if (g_dvfs_data == NULL) {
		dev_err(dev, "failed alloc mem\n");
		return -ENOMEM;
	}

	g_dvfs_data->dev = dev;
	g_dvfs_data->freq_num = freq_num;
	mutex_init(&g_dvfs_data->sync_mutex);
	init_completion(&g_dvfs_data->reg_callback_done);
	g_dvfs_data->gov_callback = &g_gov_callback;
	ddr_cur_step_g = ddr_step_list_init(DDR_DB_NODE_NUM);

	err = 0;
	for (i = 0; i < g_dvfs_data->freq_num; i++) {
		g_dvfs_data->paras[i].overflow = PARSE_FLOW_ERR;
		if (err == 0)
			err = of_property_read_u32_index(node, "overflow",
							 i, &g_dvfs_data->paras[i].overflow);

		g_dvfs_data->paras[i].underflow = PARSE_FLOW_ERR;
		if (err_uf == 0)
			err_uf = of_property_read_u32_index(node, "underflow", i,
							    &g_dvfs_data->paras[i].underflow);
	}
	if (err)
		dev_warn(dev, "could not parse freq overflow, use default settings\n");

	if (err_uf)
		dev_warn(dev, "could not parse freq underflow, use default settings\n");


	INIT_DELAYED_WORK(&g_dvfs_data->topfreq_unvote_work, topfreq_unvote_work_handler);

	set_profile(g_dvfs_data->profile);
	g_dvfs_data->dvfs_smsg_ch_open = kthread_run(dvfs_smsg_thread, g_dvfs_data, "dvfs-init");
	if (IS_ERR(g_dvfs_data->dvfs_smsg_ch_open)) {
		g_dvfs_data->dvfs_smsg_ch_open = NULL;
		return -EINVAL;
	}

	g_dvfs_data->wake_lock = wakeup_source_register(g_dvfs_data->dev, dev_name(dev));
	if (!g_dvfs_data->wake_lock) {
		dev_err(dev, "register wakeup lock fail\n");
		return -EINVAL;
	}

	platform_set_drvdata(pdev, g_dvfs_data);
	ddrinfo_dfs_step_debug_init();

	return 0;
}

int dvfs_core_clear(struct platform_device *pdev)
{
	if (g_dvfs_data->dvfs_smsg_thread_process == 1) {
		kthread_stop(g_dvfs_data->dvfs_smsg_ch_open);
		g_dvfs_data->dvfs_smsg_thread_process = 0;
		g_dvfs_data->dvfs_smsg_ch_open = NULL;
	}

	wakeup_source_unregister(g_dvfs_data->wake_lock);
	devfreq_remove_device(g_dvfs_data->devfreq);
	sprd_dvfs_del_governor();
	atomic_notifier_chain_unregister(&panic_notifier_list, &ddr_dvfs_event_nb);

	return 0;
}

void dvfs_core_hw_callback_register(struct dvfs_hw_callback *hw_callback)
{
	g_dvfs_data->hw_callback = hw_callback;
	complete(&g_dvfs_data->reg_callback_done);
}

void dvfs_core_hw_callback_clear(struct dvfs_hw_callback *hw_callback)
{
	if (g_dvfs_data->hw_callback != hw_callback)
		dev_err(g_dvfs_data->dev, "call clear err!\n");
	g_dvfs_data->hw_callback = NULL;
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("dvfs driver for sprd ddrc v1 and later");
